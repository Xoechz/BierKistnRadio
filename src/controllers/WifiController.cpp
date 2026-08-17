#include "WifiController.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
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
const QString kSettingsInterface =
    QStringLiteral("org.freedesktop.NetworkManager.Settings");
const QString kSettingsPath =
    QStringLiteral("/org/freedesktop/NetworkManager/Settings");
const QString kActiveConnectionInterface =
    QStringLiteral("org.freedesktop.NetworkManager.Connection.Active");
const QString kDeviceTypeProp = QStringLiteral("DeviceType");
const QString kSsidProp = QStringLiteral("Ssid");
const QString kStrengthProp = QStringLiteral("Strength");
const QString kFlagsProp = QStringLiteral("Flags");
const QString kWpaFlagsProp = QStringLiteral("WpaFlags");
const QString kRsnFlagsProp = QStringLiteral("RsnFlags");
const QString kSpecificObjectProp = QStringLiteral("SpecificObject");
const QString kPrimaryConnectionProp = QStringLiteral("PrimaryConnection");
const QString kPropertiesChangedSignal = QStringLiteral("PropertiesChanged");

const quint32 kNmDeviceTypeEthernet = 1;
const quint32 kNmDeviceTypeWifi = 2;

const quint32 kApPrivacyFlag = 0x00000001;
} // namespace

WifiController::WifiController(QObject *parent) : QObject(parent) {
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
                         const QList<QVariant> args =
                             watcher->reply().arguments();
                         if (!args.isEmpty()) {
                           reply = args.first();
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
}

bool WifiController::connected() const { return m_connected; }
QString WifiController::ssid() const { return m_ssid; }
QString WifiController::errorMessage() const { return m_errorMessage; }
int WifiController::signalStrength() const { return m_signalStrength; }
QVariantList WifiController::networks() const { return m_networks; }

void WifiController::setDbusCallableForTest(const DbusCallable &callable) {
  m_dbusCall = callable;
}

QString WifiController::ssidFromVariant(const QVariant &ssidVariant) {
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
  m_accessPoints.clear();
  rebuildNetworks();
  discoverWifiDevice();
}

void WifiController::discoverWifiDevice() {
  m_dbusCall(kNetworkManagerService, kNetworkManagerPath,
             kNetworkManagerInterface, QStringLiteral("GetDevices"),
             QVariantList(), [this](const QVariant &reply, const QString &) {
               QList<QDBusObjectPath> devices =
                   reply.value<QList<QDBusObjectPath>>();
               if (devices.isEmpty() && reply.canConvert<QDBusArgument>()) {
                 const QDBusArgument arg = reply.value<QDBusArgument>();
                 arg.beginArray();
                 while (!arg.atEnd()) {
                   QDBusObjectPath path;
                   arg >> path;
                   devices.append(path);
                 }
                 arg.endArray();
               }
               findWifiDevice(devices, 0);
             });
}

void WifiController::findWifiDevice(const QList<QDBusObjectPath> &devices,
                                    int index) {
  if (index >= devices.size()) {
    setError(QStringLiteral("No Wi-Fi adapter found — check system config"));
    return;
  }

  const QString devicePath = devices.at(index).path();
  m_dbusCall(kNetworkManagerService, devicePath, kPropertiesInterface,
             QStringLiteral("Get"),
             QVariantList{kDeviceInterface, kDeviceTypeProp},
             [this, devices, index, devicePath](const QVariant &reply,
                                                const QString &) {
               const uint type = reply.toUInt();
               if (type == kNmDeviceTypeWifi) {
                 m_wifiDevicePath = devicePath;
                 subscribeAccessPoints(devicePath);
                 requestScan(devicePath);
               } else {
                 findWifiDevice(devices, index + 1);
               }
             });
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
  m_dbusCall(kNetworkManagerService, devicePath, kWirelessInterface,
             QStringLiteral("RequestScan"), QVariantList{QVariantMap()},
             [](const QVariant &, const QString &) {});
}

void WifiController::connect(const QString &ssid, const QString &password) {
  // Decide security from the scanned network (if known); unknown networks are
  // assumed WPA-PSK.
  bool secured = true;
  for (const QVariant &entry : m_networks) {
    const QVariantMap network = entry.toMap();
    if (network.value(QStringLiteral("ssid")).toString() == ssid) {
      secured = network.value(QStringLiteral("secured")).toBool();
      break;
    }
  }

  const QVariantMap settings = connectionSettings(ssid, password, secured);
  m_dbusCall(kNetworkManagerService, kSettingsPath, kSettingsInterface,
             QStringLiteral("AddAndConnectConnection"), QVariantList{settings},
             [this](const QVariant &, const QString &error) {
               if (error.isEmpty()) {
                 setError(QString());
               } else {
                 // D-Bus auth/connection failures are surfaced, never silent
                 // (ADR 0002: "Permission denied — check system config").
                 setError(QStringLiteral(
                     "Wi-Fi connect failed — check system config"));
               }
             });
}

void WifiController::disconnect() {
  // Best-effort: disconnect all active connections on the wifi device.
  if (m_wifiDevicePath.isEmpty()) {
    return;
  }
  QDBusConnection::systemBus().call(QDBusMessage::createMethodCall(
      kNetworkManagerService, m_wifiDevicePath, kDeviceInterface,
      QStringLiteral("Disconnect")));
}

void WifiController::setError(const QString &message) {
  if (m_errorMessage != message) {
    m_errorMessage = message;
    emit errorMessageChanged();
  }
}

void WifiController::refreshActiveConnection() {
  m_dbusCall(kNetworkManagerService, kNetworkManagerPath,
             kNetworkManagerInterface, QStringLiteral("GetPrimaryConnection"),
             QVariantList(),
             [this](const QVariant &reply, const QString &error) {
               if (!error.isEmpty()) {
                 return;
               }
               onPrimaryConnectionChanged(reply.value<QDBusObjectPath>());
             });
}

void WifiController::onPrimaryConnectionChanged(const QDBusObjectPath &path) {
  if (path.path().isEmpty()) {
    m_primaryConnectionPath.clear();
    setConnectedState(false, QString(), 0);
    return;
  }

  // The active Wi-Fi connection's SpecificObject is the access point path;
  // read its SSID and strength to populate connected/ssid/signalStrength.
  m_primaryConnectionPath = path.path();
  m_dbusCall(kNetworkManagerService, m_primaryConnectionPath,
             kPropertiesInterface, QStringLiteral("Get"),
             QVariantList{kActiveConnectionInterface, kSpecificObjectProp},
             [this](const QVariant &specificObjectReply,
                    const QString &specificObjectError) {
               if (!specificObjectError.isEmpty()) {
                 return;
               }
               const QString apPath =
                   specificObjectReply.value<QDBusObjectPath>().path();
               if (apPath.isEmpty()) {
                 setConnectedState(false, QString(), 0);
                 return;
               }
               fetchAccessPointState(apPath);
             });
}

void WifiController::fetchAccessPointState(const QString &apPath) {
  m_dbusCall(
      kNetworkManagerService, apPath, kPropertiesInterface,
      QStringLiteral("Get"), QVariantList{kAccessPointInterface, kSsidProp},
      [this, apPath](const QVariant &ssidReply, const QString &ssidError) {
        if (!ssidError.isEmpty()) {
          return;
        }
        const QString ssid = ssidFromVariant(ssidReply);
        m_dbusCall(kNetworkManagerService, apPath, kPropertiesInterface,
                   QStringLiteral("Get"),
                   QVariantList{kAccessPointInterface, kStrengthProp},
                   [this, ssid](const QVariant &strengthReply,
                                const QString &strengthError) {
                     if (!strengthError.isEmpty()) {
                       return;
                     }
                     const QVariantMap props{{kStrengthProp, strengthReply}};
                     setConnectedState(true, ssid, accessPointStrength(props));
                   });
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
  fetchAccessPoint(path.path());
}

void WifiController::fetchAccessPoint(const QString &apPath) {
  m_dbusCall(kNetworkManagerService, apPath, kPropertiesInterface,
             QStringLiteral("GetAll"), QVariantList{kAccessPointInterface},
             [this, apPath](const QVariant &reply, const QString &) {
               accessPointAddedForTest(apPath, reply.toMap());
             });
}

void WifiController::onAccessPointRemoved(const QDBusObjectPath &path) {
  accessPointRemovedForTest(path.path());
}

void WifiController::onPropertiesChanged(
    const QString &interface, const QVariantMap &changedProperties,
    const QStringList &invalidatedProperties) {
  if (interface != kNetworkManagerInterface) {
    return;
  }
  if (changedProperties.contains(kPrimaryConnectionProp)) {
    onPrimaryConnectionChanged(changedProperties.value(kPrimaryConnectionProp)
                                   .value<QDBusObjectPath>());
  }
  if (invalidatedProperties.contains(kPrimaryConnectionProp)) {
    onPrimaryConnectionChanged(QDBusObjectPath(QString()));
  }
}