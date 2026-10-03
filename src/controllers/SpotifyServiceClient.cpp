#include "SpotifyServiceClient.h"
#include "ControllerError.h"

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>

namespace {
const QString service = QStringLiteral("org.freedesktop.systemd1");
const QString managerPath = QStringLiteral("/org/freedesktop/systemd1");
const QString managerInterface = QStringLiteral("org.freedesktop.systemd1.Manager");
QDBusMessage managerCall(const QString &method) {
  return QDBusMessage::createMethodCall(service, managerPath, managerInterface, method);
}
QString replyError(const QDBusPendingCallWatcher *watcher) {
  return watcher->error().name() + QStringLiteral(": ") + watcher->error().message();
}
} // namespace

SpotifyServiceClient::SpotifyServiceClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent), m_bus(bus) {
  m_poll.setInterval(500);
  connect(&m_poll, &QTimer::timeout, this, &SpotifyServiceClient::refresh);
  auto *watcher = new QDBusServiceWatcher(service, bus,
      QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &, const QString &) {
    ++m_generation;
    m_readPending = false;
    m_known = false;
    m_state.clear();
    m_path.clear();
    refresh();
  });
  m_poll.start();
  refresh();
}

void SpotifyServiceClient::refresh() {
  if (m_readPending) {
    return;
  }
  m_readPending = true;
  const quint64 generation = ++m_generation;
  if (!m_path.isEmpty()) {
    readState(generation);
    return;
  }
  auto call = managerCall(QStringLiteral("LoadUnit"));
  call << QStringLiteral("spotifyd.service");
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation]() {
    watcher->deleteLater();
    if (generation != m_generation) {
      return;
    }
    if (watcher->isError()) {
      readFailed(replyError(watcher));
      return;
    }
    if (watcher->reply().signature() != QStringLiteral("o")) {
      readFailed(QStringLiteral("invalid LoadUnit reply"));
      return;
    }
    m_path = watcher->reply().arguments().first().value<QDBusObjectPath>().path();
    readState(generation);
  });
}

void SpotifyServiceClient::readState(quint64 generation) {
  auto call = QDBusMessage::createMethodCall(service, m_path,
      QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll"));
  call << QStringLiteral("org.freedesktop.systemd1.Unit");
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation]() {
    watcher->deleteLater();
    if (generation != m_generation) {
      return;
    }
    if (watcher->isError()) {
      m_path.clear();
      readFailed(replyError(watcher));
      return;
    }
    if (watcher->reply().signature() != QStringLiteral("a{sv}")) {
      readFailed(QStringLiteral("invalid unit state reply"));
      return;
    }
    const auto props = qdbus_cast<QVariantMap>(watcher->reply().arguments().first());
    const QString state = props.value(QStringLiteral("ActiveState")).toString();
    if (!QStringList{QStringLiteral("active"), QStringLiteral("inactive"),
                     QStringLiteral("failed"), QStringLiteral("activating"),
                     QStringLiteral("deactivating"), QStringLiteral("reloading"),
                     QStringLiteral("refreshing"), QStringLiteral("maintenance")}.contains(state)) {
      readFailed(QStringLiteral("invalid ActiveState"));
      return;
    }
    m_readPending = false;
    m_known = true;
    m_state = state;
    m_error.clear();
    emit stateChanged();
  });
}

void SpotifyServiceClient::readFailed(const QString &error) {
  m_readPending = false;
  m_known = false;
  m_error = controllerErrorText(QStringLiteral("Spotify service status"), error);
  emit stateChanged();
}

void SpotifyServiceClient::requestRunning(bool running,
    const std::function<void(const QString &)> &finished) {
  // In-flight reads from before the requested job cannot confirm its result.
  ++m_generation;
  m_readPending = false;
  m_known = false;
  auto call = managerCall(running ? QStringLiteral("StartUnit") : QStringLiteral("StopUnit"));
  call << QStringLiteral("spotifyd.service") << QStringLiteral("replace");
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, running, finished]() {
    watcher->deleteLater();
    QString error;
    if (watcher->isError()) {
      error = replyError(watcher);
    } else if (watcher->reply().signature() != QStringLiteral("o")) {
      error = QStringLiteral("invalid systemd job reply");
    }
    // A job path acknowledges the request; only a subsequent state read
    // establishes whether the process has actually stopped or started.
    ++m_generation;
    m_readPending = false;
    m_known = false;
    finished(error.isEmpty() ? QString() : controllerErrorText(
        running ? QStringLiteral("Spotify startup") : QStringLiteral("Spotify shutdown"), error));
    refresh();
  });
}
