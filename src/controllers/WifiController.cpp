#include "WifiController.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QPointer>
#include <QTimer>
#include <QUuid>

namespace {
const QString kNetworkManagerService =
    QStringLiteral("org.freedesktop.NetworkManager");
const QString kNetworkManagerPath =
    QStringLiteral("/org/freedesktop/NetworkManager");
const QString kNetworkManagerInterface =
    QStringLiteral("org.freedesktop.NetworkManager");
const QString kDeviceInterface =
    QStringLiteral("org.freedesktop.NetworkManager.Device");
const QString kWirelessInterface =
    QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
const QString kAccessPointInterface =
    QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");
const QString kPropertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");
const QString kDeviceTypeProp = QStringLiteral("DeviceType");
const QString kSsidProp = QStringLiteral("Ssid");
const QString kStrengthProp = QStringLiteral("Strength");
const QString kFlagsProp = QStringLiteral("Flags");
const QString kWpaFlagsProp = QStringLiteral("WpaFlags");
const QString kRsnFlagsProp = QStringLiteral("RsnFlags");
const QString kPropertiesChangedSignal = QStringLiteral("PropertiesChanged");
const QString kActiveAccessPointProp = QStringLiteral("ActiveAccessPoint");
const QString kLastScanProp = QStringLiteral("LastScan");
const quint32 kNmDeviceTypeWifi = 2;
const uint kNmDeviceActivated = 100;
const uint kNmDeviceFailed = 120;
const uint kNmDeviceDisconnected = 30;
constexpr int kConnectionTimeoutMs = 30000;

const quint32 kApPrivacyFlag = 0x00000001;

QList<QDBusObjectPath> objectPaths(const QVariant &reply) {
  if (reply.canConvert<QList<QDBusObjectPath>>()) {
    return reply.value<QList<QDBusObjectPath>>();
  }
  QList<QDBusObjectPath> paths;
  if (reply.canConvert<QDBusArgument>()) {
    const QDBusArgument arg = reply.value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
      QDBusObjectPath path;
      arg >> path;
      paths.append(path);
    }
    arg.endArray();
  }
  return paths;
}

QVariantMap properties(const QVariant &reply) {
  QVariantMap result;
  if (reply.canConvert<QDBusArgument>()) {
    result = qdbus_cast<QVariantMap>(reply.value<QDBusArgument>());
  } else {
    result = reply.toMap();
  }
  for (auto it = result.begin(); it != result.end(); ++it) {
    if (it.value().canConvert<QDBusVariant>()) {
      it.value() = it.value().value<QDBusVariant>().variant();
    }
  }
  return result;
}
} // namespace

WifiController::WifiController(QObject *parent) : QObject(parent) {
  qDBusRegisterMetaType<SettingsMap>();
  m_connectTimer.setSingleShot(true);
  m_connectTimer.setInterval(kConnectionTimeoutMs);
  QObject::connect(&m_connectTimer, &QTimer::timeout, this, [this]() {
    failConnection(QStringLiteral("Wi-Fi connection timed out"));
  });
  m_dbusCall = [](const QString &service, const QString &objectPath,
                  const QString &interface, const QString &method,
                  const QVariantList &args,
                  const std::function<void(const QVariant &reply,
                                           const QString &error)> &onFinished) {
    QDBusMessage msg =
        QDBusMessage::createMethodCall(service, objectPath, interface, method);
    msg.setArguments(args);
    QDBusPendingCall pending = QDBusConnection::systemBus().asyncCall(msg);
    auto *watcher = new QDBusPendingCallWatcher(pending);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                      [watcher, onFinished]() {
                       QString error;
                       QVariant reply;
                       if (watcher->isError()) {
                         error = watcher->error().message();
                       } else {
                          const QList<QVariant> args = watcher->reply().arguments();
                          if (!args.isEmpty()) {
                            reply = args.first();
                            if (reply.canConvert<QDBusVariant>()) {
                              reply = reply.value<QDBusVariant>().variant();
                            }
                          }
                        }
                       onFinished(reply, error);
                       watcher->deleteLater();
                     });
  };

  QDBusConnection::systemBus().connect(
      kNetworkManagerService, kNetworkManagerPath, kPropertiesInterface,
      kPropertiesChangedSignal, this,
      SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

  // Find the Wi-Fi device at startup, even when Ethernet owns the default route.
  QTimer::singleShot(0, this, [this]() {
    if (m_wifiDevicePath.isEmpty()) {
      discoverWifiDevice();
    }
  });
}

bool WifiController::connected() const { return m_connected; }
QString WifiController::ssid() const { return m_ssid; }
QString WifiController::errorMessage() const { return m_errorMessage; }
bool WifiController::connecting() const { return m_connecting; }
int WifiController::signalStrength() const { return m_signalStrength; }
QVariantList WifiController::networks() const { return m_networks; }

void WifiController::setDbusCallableForTest(const DbusCallable &callable) {
  m_dbusCall = callable;
}

QString WifiController::ssidFromVariant(const QVariant &ssidVariant) {
  if (ssidVariant.canConvert<QDBusArgument>()) {
    return QString::fromUtf8(qdbus_cast<QByteArray>(
        ssidVariant.value<QDBusArgument>()));
  }
  if (ssidVariant.canConvert<QByteArray>()) {
    return QString::fromUtf8(ssidVariant.toByteArray());
  }
  return ssidVariant.toString();
}

bool WifiController::accessPointSecured(const QVariantMap &props) {
  const quint32 flags = props.value(kFlagsProp).toUInt();
  const quint32 wpaFlags = props.value(kWpaFlagsProp).toUInt();
  const quint32 rsnFlags = props.value(kRsnFlagsProp).toUInt();
  return (flags & kApPrivacyFlag) != 0 || wpaFlags != 0 || rsnFlags != 0;
}

int WifiController::accessPointStrength(const QVariantMap &props) {
  return props.value(kStrengthProp).toInt();
}

QVariantMap WifiController::connectionSettings(const QString &ssid,
                                               const QString &password,
                                               bool secured) {
  QVariantMap profile;
  QVariantMap connection;
  connection.insert(QStringLiteral("type"), QStringLiteral("802-11-wireless"));
  connection.insert(QStringLiteral("uuid"),
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
  connection.insert(QStringLiteral("id"), ssid);
  profile.insert(QStringLiteral("connection"), connection);

  QVariantMap wireless;
  wireless.insert(QStringLiteral("ssid"), QVariant(ssid.toUtf8()));
  wireless.insert(QStringLiteral("mode"), QStringLiteral("infrastructure"));

  if (secured) {
    wireless.insert(QStringLiteral("security"),
                    QStringLiteral("802-11-wireless-security"));
    QVariantMap security;
    security.insert(QStringLiteral("key-mgmt"), QStringLiteral("wpa-psk"));
    security.insert(QStringLiteral("psk"), password);
    profile.insert(QStringLiteral("802-11-wireless-security"), security);
  }

  profile.insert(QStringLiteral("802-11-wireless"), wireless);

  QVariantMap ipv4;
  ipv4.insert(QStringLiteral("method"), QStringLiteral("auto"));
  profile.insert(QStringLiteral("ipv4"), ipv4);
  QVariantMap ipv6;
  ipv6.insert(QStringLiteral("method"), QStringLiteral("auto"));
  profile.insert(QStringLiteral("ipv6"), ipv6);

  return profile;
}

void WifiController::scan() {
  setError(QString());
  ++m_scanGeneration;
  ++m_inventoryGeneration;
  m_accessPoints.clear();
  m_knownAccessPoints.clear();
  rebuildNetworks();
  if (!m_wifiDevicePath.isEmpty()) {
    refreshAccessPoints(m_wifiDevicePath, m_scanGeneration);
    requestScan(m_wifiDevicePath);
  } else {
    discoverWifiDevice();
  }
}

void WifiController::discoverWifiDevice() {
  const quint64 generation = m_scanGeneration;
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, kNetworkManagerPath,
              kNetworkManagerInterface, QStringLiteral("GetDevices"),
              QVariantList(), [self, generation](const QVariant &reply,
                                                  const QString &error) {
                if (!self || generation != self->m_scanGeneration) {
                  return;
                }
                if (!error.isEmpty()) {
                  self->setError(dbusErrorText(QStringLiteral("Wi-Fi device lookup"), error));
                  return;
                }
                self->findWifiDevice(objectPaths(reply), 0, generation);
              });
}

void WifiController::findWifiDevice(const QList<QDBusObjectPath> &devices,
                                     int index, quint64 generation) {
  if (index >= devices.size()) {
    setError(QStringLiteral("No Wi-Fi adapter found — check system config"));
    return;
  }

  const QString devicePath = devices.at(index).path();
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, devicePath, kPropertiesInterface,
              QStringLiteral("Get"),
              QVariantList{kDeviceInterface, kDeviceTypeProp},
              [self, devices, index, devicePath, generation](const QVariant &reply,
                                                              const QString &error) {
                if (!self || generation != self->m_scanGeneration) {
                  return;
                }
                if (!error.isEmpty()) {
                  self->setError(dbusErrorText(QStringLiteral("Wi-Fi device lookup"), error));
                  return;
                }
                const uint type = reply.toUInt();
                if (type == kNmDeviceTypeWifi) {
                  self->setWifiDevicePath(devicePath);
                  self->refreshAccessPoints(devicePath, generation);
                  self->requestScan(devicePath);
                } else {
                  self->findWifiDevice(devices, index + 1, generation);
                }
              });
}

void WifiController::setWifiDevicePath(const QString &path) {
  if (m_wifiDevicePath == path) {
    return;
  }
  if (!m_wifiDevicePath.isEmpty()) {
    QDBusConnection::systemBus().disconnect(kNetworkManagerService, m_wifiDevicePath,
        kWirelessInterface, QStringLiteral("AccessPointAdded"), this,
        SLOT(onAccessPointAdded(QDBusObjectPath)));
    QDBusConnection::systemBus().disconnect(kNetworkManagerService, m_wifiDevicePath,
        kWirelessInterface, QStringLiteral("AccessPointRemoved"), this,
        SLOT(onAccessPointRemoved(QDBusObjectPath)));
    QDBusConnection::systemBus().disconnect(kNetworkManagerService, m_wifiDevicePath,
        kPropertiesInterface, kPropertiesChangedSignal, this,
        SLOT(onWifiPropertiesChanged(QString, QVariantMap, QStringList)));
    QDBusConnection::systemBus().disconnect(kNetworkManagerService, m_wifiDevicePath,
        kDeviceInterface, QStringLiteral("StateChanged"), this,
        SLOT(onWifiDeviceStateChanged(uint, uint, uint)));
  }
  m_wifiDevicePath = path;
  subscribeAccessPoints(path);
  QDBusConnection::systemBus().connect(kNetworkManagerService, path,
      kPropertiesInterface, kPropertiesChangedSignal, this,
      SLOT(onWifiPropertiesChanged(QString, QVariantMap, QStringList)));
  QDBusConnection::systemBus().connect(kNetworkManagerService, path,
      kDeviceInterface, QStringLiteral("StateChanged"), this,
      SLOT(onWifiDeviceStateChanged(uint, uint, uint)));
  refreshActiveConnection();
}

void WifiController::subscribeAccessPoints(const QString &devicePath) {
  QDBusConnection::systemBus().connect(
      kNetworkManagerService, devicePath, kWirelessInterface,
      QStringLiteral("AccessPointAdded"), this,
      SLOT(onAccessPointAdded(QDBusObjectPath)));
  QDBusConnection::systemBus().connect(
      kNetworkManagerService, devicePath, kWirelessInterface,
      QStringLiteral("AccessPointRemoved"), this,
      SLOT(onAccessPointRemoved(QDBusObjectPath)));
}

void WifiController::requestScan(const QString &devicePath) {
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, devicePath, kWirelessInterface,
              QStringLiteral("RequestScan"), QVariantList{QVariantMap()},
              [self](const QVariant &, const QString &error) {
                if (self && !error.isEmpty()) {
                  self->setError(dbusErrorText(QStringLiteral("Wi-Fi scan"), error));
                }
              });
}

void WifiController::refreshAccessPoints(const QString &devicePath,
                                         quint64 generation) {
  const quint64 inventory = ++m_inventoryGeneration;
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, devicePath, kWirelessInterface,
              QStringLiteral("GetAllAccessPoints"), QVariantList(),
              [self, devicePath, generation, inventory](const QVariant &reply,
                                              const QString &error) {
                if (!self || generation != self->m_scanGeneration ||
                    inventory != self->m_inventoryGeneration ||
                    devicePath != self->m_wifiDevicePath) {
                  return;
                }
                if (!error.isEmpty()) {
                  self->setError(dbusErrorText(QStringLiteral("Wi-Fi list"), error));
                  return;
                }
                QSet<QString> seen;
                for (const QDBusObjectPath &path : objectPaths(reply)) {
                  seen.insert(path.path());
                }
                for (auto it = self->m_accessPoints.begin();
                     it != self->m_accessPoints.end();) {
                  if (!seen.contains(it.key())) {
                    it = self->m_accessPoints.erase(it);
                  } else {
                    ++it;
                  }
                }
                self->m_knownAccessPoints = seen;
                self->rebuildNetworks();
                for (const QString &path : seen) {
                  self->fetchAccessPoint(path, generation);
                }
              });
}

void WifiController::connect(const QString &ssid, const QString &password) {
  if (m_wifiDevicePath.isEmpty() || ssid.isEmpty()) {
    setError(QStringLiteral("Select a Wi-Fi network after scanning"));
    return;
  }
  setError(QString());
  // Decide security from the scanned network (if known); unknown networks are
  // assumed WPA-PSK.
  bool secured = true;
  QString apPath;
  for (const QVariant &entry : m_networks) {
    const QVariantMap network = entry.toMap();
    if (network.value(QStringLiteral("ssid")).toString() == ssid) {
      secured = network.value(QStringLiteral("secured")).toBool();
      break;
    }
  }

  int bestStrength = -1;
  for (auto it = m_accessPoints.cbegin(); it != m_accessPoints.cend(); ++it) {
    const QVariantMap props = it.value().toMap();
    if (ssidFromVariant(props.value(kSsidProp)) == ssid &&
        accessPointStrength(props) > bestStrength) {
      apPath = it.key();
      bestStrength = accessPointStrength(props);
    }
  }

  m_connectRequestedSsid = ssid;
  setConnecting(true);
  const quint64 attempt = ++m_connectGeneration;
  m_connectTimer.start();
  SettingsMap profile;
  const QVariantMap settings = connectionSettings(ssid, password, secured);
  for (auto it = settings.cbegin(); it != settings.cend(); ++it) {
    profile.insert(it.key(), it.value().toMap());
  }
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, kNetworkManagerPath,
             kNetworkManagerInterface, QStringLiteral("AddAndActivateConnection"),
             QVariantList{QVariant::fromValue(profile),
                          QVariant::fromValue(QDBusObjectPath(m_wifiDevicePath)),
                          QVariant::fromValue(QDBusObjectPath(
                              apPath.isEmpty() ? QStringLiteral("/") : apPath))},
             [self, ssid, attempt](const QVariant &, const QString &error) {
               if (self && self->m_connectGeneration == attempt &&
                   self->m_connectRequestedSsid == ssid &&
                   !error.isEmpty()) {
                 self->failConnection(dbusErrorText(QStringLiteral("Wi-Fi connect"), error));
               }
             });
}

void WifiController::failConnection(const QString &message) {
  m_connectTimer.stop();
  m_connectRequestedSsid.clear();
  setConnecting(false);
  setError(message);
}

QString WifiController::dbusErrorText(const QString &operation,
                                      const QString &error) {
  if (error.contains(QStringLiteral("AccessDenied"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("NotAuthorized"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("permission"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("privilege"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("authoriz"), Qt::CaseInsensitive)) {
    return QStringLiteral("Permission denied — check system config");
  }
  return operation + QStringLiteral(" failed: ") + error;
}

void WifiController::disconnect() {
  // Best-effort: disconnect all active connections on the wifi device.
  if (m_wifiDevicePath.isEmpty()) {
    return;
  }
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, m_wifiDevicePath, kDeviceInterface,
             QStringLiteral("Disconnect"), QVariantList(),
             [self](const QVariant &, const QString &error) {
               if (self && !error.isEmpty()) {
                 self->setError(dbusErrorText(QStringLiteral("Wi-Fi disconnect"), error));
               }
             });
}

void WifiController::setError(const QString &message) {
  if (m_errorMessage != message) {
    m_errorMessage = message;
    emit errorMessageChanged();
  }
}

void WifiController::setConnecting(bool connecting) {
  if (m_connecting != connecting) {
    m_connecting = connecting;
    emit connectingChanged();
  }
}

void WifiController::refreshActiveConnection() {
  if (m_wifiDevicePath.isEmpty()) {
    return;
  }
  const quint64 generation = ++m_stateGeneration;
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, m_wifiDevicePath, kPropertiesInterface,
             QStringLiteral("Get"),
             QVariantList{kDeviceInterface, QStringLiteral("State")},
             [self, generation](const QVariant &reply, const QString &error) {
               if (!self || generation != self->m_stateGeneration) {
                 return;
               }
               if (!error.isEmpty()) {
                 self->setError(dbusErrorText(QStringLiteral("Wi-Fi status"), error));
               } else {
                 self->onWifiDeviceStateChanged(reply.toUInt(), 0, 0);
               }
             });
}

void WifiController::onWifiDeviceStateChanged(uint newState, uint, uint reason) {
  ++m_stateGeneration;
  if (newState == kNmDeviceFailed) {
    setConnectedState(false, QString(), 0);
    if (!m_connectRequestedSsid.isEmpty()) {
      failConnection(QStringLiteral("Wi-Fi connection failed (reason %1)")
                         .arg(reason));
    }
    return;
  }
  if (newState == kNmDeviceDisconnected) {
    setConnectedState(false, QString(), 0);
    return;
  }
  if (newState != kNmDeviceActivated) {
    return;
  }
  const quint64 generation = m_stateGeneration;
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, m_wifiDevicePath, kPropertiesInterface,
             QStringLiteral("Get"),
             QVariantList{kWirelessInterface, kActiveAccessPointProp},
             [self, generation](const QVariant &reply, const QString &error) {
               if (!self || generation != self->m_stateGeneration) {
                 return;
               }
               if (!error.isEmpty()) {
                 self->setError(dbusErrorText(QStringLiteral("Wi-Fi status"), error));
                 return;
               }
               const QString path = reply.value<QDBusObjectPath>().path();
               if (path.isEmpty() || path == QStringLiteral("/")) {
                 self->setConnectedState(false, QString(), 0);
                 return;
               }
               self->fetchAccessPointState(path);
             });
}

void WifiController::fetchAccessPointState(const QString &apPath) {
  const quint64 generation = m_stateGeneration;
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, apPath, kPropertiesInterface,
             QStringLiteral("GetAll"), QVariantList{kAccessPointInterface},
             [self, generation](const QVariant &reply, const QString &error) {
               if (!self || generation != self->m_stateGeneration) {
                 return;
               }
               if (!error.isEmpty()) {
                 self->setError(dbusErrorText(QStringLiteral("Wi-Fi status"), error));
                 return;
               }
               const QVariantMap props = properties(reply);
               const QString ssid = ssidFromVariant(props.value(kSsidProp));
               if (ssid.isEmpty()) {
                 return;
               }
               self->setConnectedState(true, ssid, accessPointStrength(props));
               if (!self->m_connectRequestedSsid.isEmpty()) {
                 if (ssid == self->m_connectRequestedSsid) {
                   self->m_connectTimer.stop();
                   self->m_connectRequestedSsid.clear();
                   self->setConnecting(false);
                   self->setError(QString());
                   emit self->connectionSucceeded(ssid);
                 } else {
                   self->failConnection(QStringLiteral("Connected to a different Wi-Fi network"));
                 }
               }
             });
}

void WifiController::setConnectedState(bool connected, const QString &ssid,
                                       int signalStrength) {
  if (m_connected != connected) {
    m_connected = connected;
    emit connectedChanged();
  }
  if (m_ssid != ssid) {
    m_ssid = ssid;
    emit ssidChanged();
  }
  if (m_signalStrength != signalStrength) {
    m_signalStrength = signalStrength;
    emit signalStrengthChanged();
  }
}

void WifiController::accessPointAddedForTest(const QString &apPath,
                                             const QVariantMap &props) {
  m_accessPoints.insert(apPath, props);
  rebuildNetworks();
}

void WifiController::accessPointRemovedForTest(const QString &apPath) {
  m_accessPoints.remove(apPath);
  rebuildNetworks();
}

void WifiController::rebuildNetworks() {
  // Keyed by SSID: keep the strongest AP per SSID, and mark the network
  // secured if any of its APs has security flags.
  QMap<QString, QVariantMap> strongestBySsid;
  for (const QVariant &variant : m_accessPoints) {
    const QVariantMap props = variant.toMap();
    const QString ssid = ssidFromVariant(props.value(kSsidProp));
    if (ssid.isEmpty()) {
      continue;
    }
    const QVariantMap existing = strongestBySsid.value(ssid);
    if (existing.isEmpty() || props.value(kStrengthProp).toInt() >
                                  existing.value(kStrengthProp).toInt()) {
      strongestBySsid.insert(ssid, props);
    } else if (accessPointSecured(props) && !accessPointSecured(existing)) {
      QVariantMap merged = existing;
      merged.insert(kRsnFlagsProp, QVariant::fromValue(1u)); // mark secured
      strongestBySsid.insert(ssid, merged);
    }
  }

  QVariantList networks;
  for (auto it = strongestBySsid.constBegin(); it != strongestBySsid.constEnd();
       ++it) {
    QVariantMap entry;
    entry.insert(QStringLiteral("ssid"), it.key());
    entry.insert(QStringLiteral("signalStrength"),
                 it.value().value(kStrengthProp).toInt());
    entry.insert(QStringLiteral("secured"), accessPointSecured(it.value()));
    networks.append(entry);
  }

  if (m_networks != networks) {
    m_networks = networks;
    emit networksChanged();
  }
}

void WifiController::onAccessPointAdded(const QDBusObjectPath &path) {
  m_knownAccessPoints.insert(path.path());
  fetchAccessPoint(path.path(), m_scanGeneration);
}

void WifiController::fetchAccessPoint(const QString &apPath, quint64 generation) {
  QPointer<WifiController> self(this);
  m_dbusCall(kNetworkManagerService, apPath, kPropertiesInterface,
              QStringLiteral("GetAll"), QVariantList{kAccessPointInterface},
              [self, apPath, generation](const QVariant &reply,
                                          const QString &error) {
                if (!self || generation != self->m_scanGeneration ||
                    !self->m_knownAccessPoints.contains(apPath)) {
                  return;
                }
                if (!error.isEmpty()) {
                  self->setError(dbusErrorText(QStringLiteral("Wi-Fi list"), error));
                  return;
                }
                self->accessPointAddedForTest(apPath, properties(reply));
              });
}

void WifiController::onAccessPointRemoved(const QDBusObjectPath &path) {
  m_knownAccessPoints.remove(path.path());
  accessPointRemovedForTest(path.path());
}

void WifiController::onPropertiesChanged(
    const QString &interface, const QVariantMap &changedProperties,
    const QStringList &invalidatedProperties) {
  Q_UNUSED(changedProperties)
  Q_UNUSED(invalidatedProperties)
  if (interface == kNetworkManagerInterface && m_wifiDevicePath.isEmpty()) {
    discoverWifiDevice();
  }
}

void WifiController::onWifiPropertiesChanged(
    const QString &interface, const QVariantMap &changedProperties,
    const QStringList &invalidatedProperties) {
  if (interface == kWirelessInterface &&
      (changedProperties.contains(kLastScanProp) ||
       invalidatedProperties.contains(kLastScanProp))) {
    refreshAccessPoints(m_wifiDevicePath, m_scanGeneration);
  }
  if (interface == kWirelessInterface &&
      (changedProperties.contains(kActiveAccessPointProp) ||
       invalidatedProperties.contains(kActiveAccessPointProp))) {
    refreshActiveConnection();
  }
}
