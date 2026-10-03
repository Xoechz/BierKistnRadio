#include "PairingAgent.h"
#include "ControllerError.h"

#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

namespace {
const QString agentPath = QStringLiteral("/org/bierkistn/PairingAgent");
QDBusMessage managerCall(const QString &method) {
  auto call = QDBusMessage::createMethodCall(
      QStringLiteral("org.bluez"), QStringLiteral("/org/bluez"),
      QStringLiteral("org.bluez.AgentManager1"), method);
  call << QVariant::fromValue(QDBusObjectPath(agentPath));
  return call;
}
} // namespace

PairingAgent::PairingAgent(const QDBusConnection &bus, QObject *parent)
    : QObject(parent), m_bus(bus) {
  m_exported = m_bus.registerObject(agentPath, this,
                                  QDBusConnection::ExportScriptableSlots);
  m_deadline.setSingleShot(true);
  m_deadline.setTimerType(Qt::PreciseTimer);
  m_deadline.setInterval(30000);
  m_countdown.setInterval(1000);
  connect(&m_deadline, &QTimer::timeout, this, [this]() {
    finish(false, QStringLiteral("Pairing confirmation timed out"));
  });
  connect(&m_countdown, &QTimer::timeout, this, [this]() {
    m_seconds = qMax(0, (m_deadline.remainingTime() + 999) / 1000);
    emit promptChanged();
  });
  auto *watcher = new QDBusServiceWatcher(
      QStringLiteral("org.bluez"), bus,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &, const QString &) {
    ++m_generation;
    finish(false, QStringLiteral("Bluetooth service changed"));
    m_owner.clear();
    if (m_enabled) {
      registerAgent();
    }
  });
  connect(qApp, &QCoreApplication::aboutToQuit, this,
          [this]() { setEnabled(false); });
}

PairingAgent::~PairingAgent() {
  finish(false, QStringLiteral("Application closed"));
  if (m_enabled) {
    unregisterAgent();
  }
  if (m_exported) {
    m_bus.unregisterObject(agentPath);
  }
}

void PairingAgent::setDeviceLookup(
    std::function<QString(const QString &)> name,
    std::function<bool(const QString &)> paired) {
  m_nameLookup = std::move(name);
  m_pairedLookup = std::move(paired);
}

void PairingAgent::setEnabled(bool enabled) {
  if (m_enabled == enabled) {
    return;
  }
  m_enabled = enabled;
  ++m_generation;
  if (enabled) {
    registerAgent();
  } else {
    finish(false, QStringLiteral("Bluetooth pairing is unavailable"));
    unregisterAgent();
    m_owner.clear();
    emit errorChanged(QString());
  }
}

void PairingAgent::registerAgent() {
  if (!m_exported || !m_bus.interface()) {
    emit errorChanged(QStringLiteral("Bluetooth pairing agent unavailable — check system config"));
    return;
  }
  const QDBusReply<QString> owner = m_bus.interface()->serviceOwner(QStringLiteral("org.bluez"));
  m_owner = owner.isValid() ? owner.value() : QString();
  const quint64 generation = m_generation;
  auto call = managerCall(QStringLiteral("RegisterAgent"));
  call << QStringLiteral("DisplayYesNo");
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation]() {
    watcher->deleteLater();
    if (generation != m_generation || !m_enabled) {
      return;
    }
    if (watcher->isError()) {
      emit errorChanged(controllerErrorText(QStringLiteral("Bluetooth pairing registration"),
          watcher->error().name() + QStringLiteral(": ") + watcher->error().message()));
      return;
    }
    auto *defaultWatcher = new QDBusPendingCallWatcher(
        m_bus.asyncCall(managerCall(QStringLiteral("RequestDefaultAgent")), 5000), this);
    connect(defaultWatcher, &QDBusPendingCallWatcher::finished, this,
            [this, defaultWatcher, generation]() {
      defaultWatcher->deleteLater();
      if (generation != m_generation || !m_enabled) {
        return;
      }
      emit errorChanged(defaultWatcher->isError()
          ? controllerErrorText(QStringLiteral("Bluetooth default pairing agent"),
              defaultWatcher->error().name() + QStringLiteral(": ") + defaultWatcher->error().message())
          : QString());
    });
  });
}

void PairingAgent::unregisterAgent() {
  auto call = managerCall(QStringLiteral("UnregisterAgent"));
  call.setAutoStartService(false);
  m_bus.asyncCall(call, 5000);
}

bool PairingAgent::trustedCall() {
  if (!calledFromDBus()) {
    return false;
  }
  if (!m_enabled || m_owner.isEmpty() || message().service() != m_owner) {
    sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                   QStringLiteral("Bluetooth pairing is unavailable"));
    return false;
  }
  return true;
}

void PairingAgent::request(const QDBusObjectPath &device, uint code) {
  if (!trustedCall()) {
    return;
  }
  if (code > 999999 || pending()) {
    sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                   QStringLiteral("Another pairing request is pending or the passkey is invalid"));
    return;
  }
  setDelayedReply(true);
  emit errorChanged(QString());
  m_requests.append(message());
  if (m_requests.size() == 1) {
    ++m_requestId;
    m_device = device.path();
    m_name = m_nameLookup ? m_nameLookup(m_device) : m_device;
    m_code = QString::number(code).rightJustified(6, '0');
    m_seconds = 30;
    m_deadline.start();
    m_countdown.start();
  }
  emit promptChanged();
}

void PairingAgent::RequestConfirmation(const QDBusObjectPath &device, uint code) {
  request(device, code);
}

void PairingAgent::DisplayPasskey(const QDBusObjectPath &device, uint, ushort) {
  if (!trustedCall()) {
    return;
  }
  // BlueZ sends this as a notification, not an approval request. Holding or
  // rejecting its reply cannot prevent pairing. Fail closed: DisplayYesNo
  // phones must use RequestConfirmation for the mandatory touchscreen gate.
  cancelPairing(device.path());
  emit errorChanged(QStringLiteral(
      "Bluetooth pairing requires matching-code confirmation — passkey-entry pairing is unsupported"));
}

void PairingAgent::resolve(quint64 id, bool accept) {
  if (id == m_requestId && pending()) {
    finish(accept && m_deadline.remainingTime() > 0,
           QStringLiteral("Pairing rejected on touchscreen"));
  }
}

void PairingAgent::finish(bool accept, const QString &reason) {
  if (!pending()) {
    return;
  }
  for (const auto &request : std::as_const(m_requests)) {
    m_bus.send(accept ? request.createReply()
                     : request.createErrorReply(QStringLiteral("org.bluez.Error.Rejected"), reason));
  }
  m_requests.clear();
  m_deadline.stop();
  m_countdown.stop();
  m_name.clear();
  m_code.clear();
  m_device.clear();
  m_seconds = 0;
  emit promptChanged();
}

void PairingAgent::cancelPairing(const QString &device) {
  auto call = QDBusMessage::createMethodCall(m_owner, device,
      QStringLiteral("org.bluez.Device1"), QStringLiteral("CancelPairing"));
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
  const quint64 generation = m_generation;
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation]() {
    watcher->deleteLater();
    if (watcher->isError() && generation == m_generation) {
      emit errorChanged(controllerErrorText(QStringLiteral("Bluetooth pairing cancellation"),
          watcher->error().name() + QStringLiteral(": ") + watcher->error().message()));
    }
  });
}

void PairingAgent::Cancel() {
  if (trustedCall()) {
    finish(false, QStringLiteral("Pairing canceled by Bluetooth"));
  }
}

void PairingAgent::Release() {
  if (trustedCall()) {
    ++m_generation;
    finish(false, QStringLiteral("Pairing agent released"));
    m_owner.clear();
    emit errorChanged(QStringLiteral("Bluetooth pairing agent released — select Bluetooth again"));
  }
}

void PairingAgent::unsupported() {
  if (trustedCall()) {
    sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                   QStringLiteral("Pairing requires a displayed matching passkey"));
    emit errorChanged(QStringLiteral("Bluetooth pairing requires matching-code confirmation"));
  }
}

QString PairingAgent::RequestPinCode(const QDBusObjectPath &) { unsupported(); return {}; }
uint PairingAgent::RequestPasskey(const QDBusObjectPath &) { unsupported(); return 0; }
void PairingAgent::DisplayPinCode(const QDBusObjectPath &, const QString &) { unsupported(); }
void PairingAgent::RequestAuthorization(const QDBusObjectPath &) { unsupported(); }
void PairingAgent::AuthorizeService(const QDBusObjectPath &device, const QString &) {
  if (trustedCall() && (!m_pairedLookup || !m_pairedLookup(device.path()))) {
    sendErrorReply(QStringLiteral("org.bluez.Error.Rejected"),
                   QStringLiteral("Device has not completed pairing"));
  }
}
