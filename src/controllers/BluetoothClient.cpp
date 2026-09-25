#include "BluetoothClient.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QPointer>

// Per-object PropertiesChanged receiver. QDBusConnection::connect has no
// functor overload, and the signal carries its arguments but not the emitting
// object path, so each subscription gets its own receiver that knows its path.
class BlueZPropsSubscriber : public QObject {
  Q_OBJECT
public:
  explicit BlueZPropsSubscriber(
      std::function<void(const QString &, const QVariantMap &,
                         const QStringList &)> cb)
      : m_cb(std::move(cb)) {}

public slots:
  void onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                           const QStringList &invalidated) {
    m_cb(interface, changed, invalidated);
  }

private:
  std::function<void(const QString &, const QVariantMap &,
                     const QStringList &)> m_cb;
};

namespace {
const QString kBlueZService = QStringLiteral("org.bluez");
const QString kBlueZRoot = QStringLiteral("/");
const QString kDeviceInterface = QStringLiteral("org.bluez.Device1");
const QString kMediaPlayerInterface = QStringLiteral("org.bluez.MediaPlayer1");
const QString kAdapterInterface = QStringLiteral("org.bluez.Adapter1");
const QString kObjectManagerInterface =
    QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString kPropertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");
const QString kPropertiesSignal = QStringLiteral("PropertiesChanged");

const QString kConnectedProp = QStringLiteral("Connected");
const QString kNameProp = QStringLiteral("Name");
const QString kAliasProp = QStringLiteral("Alias");
const QString kAddressProp = QStringLiteral("Address");
const QString kAdapterProp = QStringLiteral("Adapter");
const QString kDiscoverableProp = QStringLiteral("Discoverable");
const QString kPoweredProp = QStringLiteral("Powered");
const QString kPairableProp = QStringLiteral("Pairable");

const QString kStatusProp = QStringLiteral("Status");
const QString kTrackProp = QStringLiteral("Track");
const QString kPositionProp = QStringLiteral("Position");
const QString kDurationKey = QStringLiteral("Duration");
constexpr int kTakeoverDisconnectTimeoutMs = 5000;

QVariant unwrapDbusVariant(const QVariant &value) {
  if (value.canConvert<QDBusVariant>()) {
    return value.value<QDBusVariant>().variant();
  }
  return value;
}

QVariantMap decodedProperties(const QVariant &value) {
  const QVariant unwrapped = unwrapDbusVariant(value);
  QVariantMap result = unwrapped.canConvert<QDBusArgument>()
                           ? qdbus_cast<QVariantMap>(unwrapped.value<QDBusArgument>())
                           : unwrapped.toMap();
  for (auto it = result.begin(); it != result.end(); ++it) {
    it.value() = unwrapDbusVariant(it.value());
  }
  return result;
}
} // namespace

BluetoothClient::BluetoothClient(QObject *parent)
    : BluetoothClient(QDBusConnection::systemBus(), parent) {}

BluetoothClient::BluetoothClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent), m_bus(bus) {
  m_dbusCall = [bus](const QString &service, const QString &objectPath,
                  const QString &interface, const QString &method,
                  const QVariantList &args,
                  const std::function<void(const QVariant &reply,
                                           const QString &error)> &onFinished) {
    QDBusMessage msg =
        QDBusMessage::createMethodCall(service, objectPath, interface, method);
    msg.setArguments(args);
    QDBusPendingCall pending = bus.asyncCall(msg);
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

  m_runner = [](const QStringList &args,
                const std::function<void(const QByteArray &)> &onFinished) {
    auto *proc = new QProcess;
    QObject::connect(proc, &QProcess::finished, proc,
                     [proc, onFinished](int, QProcess::ExitStatus) {
                       onFinished(proc->readAllStandardOutput());
                       proc->deleteLater();
                     });
    proc->start(args.value(0), args.mid(1));
  };

  m_takeoverTimer.setSingleShot(true);
  m_takeoverTimer.setInterval(kTakeoverDisconnectTimeoutMs);
  connect(&m_takeoverTimer, &QTimer::timeout, this, [this]() {
    finishTakeoverAttempt(
        QStringLiteral("Bluetooth disconnect timed out — try again"));
  });

  // Live object additions/removals while running. `org.bluez` implements the
  // standard DBus object-manager interface at `/`.
  qDBusRegisterMetaType<QMap<QString, QVariantMap>>();
  subscribeObjectManager();
  m_serviceWatcher = new QDBusServiceWatcher(
      kBlueZService, m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
  connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
          [this](const QString &, const QString &, const QString &newOwner) {
            clearBluezState();
            if (!newOwner.isEmpty()) {
              subscribeObjectManager();
              refreshManagedObjects();
            }
          });
  refreshManagedObjects();
}

void BluetoothClient::subscribeObjectManager() {
  m_bus.disconnect(
      kBlueZService, kBlueZRoot, kObjectManagerInterface,
      QStringLiteral("InterfacesAdded"), this,
      SLOT(onInterfacesAdded(QDBusObjectPath, QMap<QString, QVariantMap>)));
  m_bus.disconnect(
      kBlueZService, kBlueZRoot, kObjectManagerInterface,
      QStringLiteral("InterfacesRemoved"), this,
      SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
  m_bus.connect(
      kBlueZService, kBlueZRoot, kObjectManagerInterface,
      QStringLiteral("InterfacesAdded"), this,
      SLOT(onInterfacesAdded(QDBusObjectPath, QMap<QString, QVariantMap>)));
  m_bus.connect(
      kBlueZService, kBlueZRoot, kObjectManagerInterface,
      QStringLiteral("InterfacesRemoved"), this,
      SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
}

void BluetoothClient::refreshManagedObjects() {
  const quint64 generation = ++m_objectManagerGeneration;
  QPointer<BluetoothClient> self(this);
  m_dbusCall(kBlueZService, kBlueZRoot, kObjectManagerInterface,
              QStringLiteral("GetManagedObjects"), QVariantList(),
              [self, generation](const QVariant &reply, const QString &error) {
                if (!self || generation != self->m_objectManagerGeneration ||
                    !error.isEmpty() || !reply.canConvert<QDBusArgument>()) {
                  return;
                }
                const QDBusArgument arg = reply.value<QDBusArgument>();
                // a{oa{sa{sv}}}: path -> { interface -> props }
                QMap<QString, QVariantMap> players;
                arg.beginMap();
                while (!arg.atEnd()) {
                  QDBusObjectPath path;
                  QMap<QString, QVariantMap> interfaces;
                  arg.beginMapEntry();
                  arg >> path;
                  arg >> interfaces;
                  arg.endMapEntry();
                  for (auto it = interfaces.cbegin(); it != interfaces.cend();
                       ++it) {
                    if (it.key() == kMediaPlayerInterface) {
                      players.insert(path.path(), it.value());
                    } else {
                      self->applyInterfaceAdded(path.path(), it.key(), it.value());
                    }
                  }
                }
                arg.endMap();
                for (auto it = players.cbegin(); it != players.cend(); ++it) {
                  self->applyInterfaceAdded(it.key(), kMediaPlayerInterface,
                                            it.value());
                }
              });
}

void BluetoothClient::clearBluezState() {
  ++m_objectManagerGeneration;
  for (const QString &path : m_propertySubscribers.keys()) {
    unsubscribeProperties(path);
  }
  m_devices.clear();
  m_connectedOrder.clear();
  m_playerOwners.clear();
  m_deferredPlayers.clear();
  m_takeoverDevicePath.clear();
  m_adapterPath.clear();
  finishTakeoverAttempt();
  setTakeoverPending(false);
  updateTakeoverIncoming();
  setActiveDevice(QString());
  setAdapterPowered(false);
  setAdapterDiscoverable(false);
  setAdapterPairable(false);
}

QString BluetoothClient::connectedDeviceName() const {
  if (m_activeDevicePath.isEmpty()) {
    return QString();
  }
  return deviceDisplayName(m_devices.value(m_activeDevicePath));
}

QString BluetoothClient::deviceDisplayName(const DeviceState &device) {
  if (!device.alias.isEmpty()) {
    return device.alias;
  }
  if (!device.name.isEmpty()) {
    return device.name;
  }
  return device.address.isEmpty() ? QStringLiteral("Bluetooth device")
                                  : device.address;
}

bool BluetoothClient::hasConnectedDevice() const {
  return !m_activeDevicePath.isEmpty();
}

bool BluetoothClient::takeoverPending() const { return m_takeoverPending; }
QString BluetoothClient::takeoverIncomingName() const {
  return m_takeoverIncomingName;
}
bool BluetoothClient::takeoverResolving() const { return m_takeoverResolving; }
QString BluetoothClient::takeoverError() const { return m_takeoverError; }
bool BluetoothClient::adapterPowered() const { return m_adapterPowered; }
bool BluetoothClient::adapterDiscoverable() const {
  return m_adapterDiscoverable;
}
bool BluetoothClient::adapterPairable() const { return m_adapterPairable; }

bool BluetoothClient::statusPublished() const { return m_statusPublished; }
bool BluetoothClient::trackPublished() const { return m_trackPublished; }
QString BluetoothClient::trackTitle() const { return m_trackTitle; }
QString BluetoothClient::trackArtist() const { return m_trackArtist; }
QString BluetoothClient::trackAlbum() const { return m_trackAlbum; }
qint64 BluetoothClient::position() const { return m_position; }
qint64 BluetoothClient::duration() const { return m_duration; }
bool BluetoothClient::positionPublished() const { return m_positionPublished; }
bool BluetoothClient::isBluetoothPlaying() const { return m_isBluetoothPlaying; }
bool BluetoothClient::muted() const { return m_muted; }

void BluetoothClient::setDbusCallableForTest(const DbusCallable &callable) {
  m_dbusCall = callable;
}

void BluetoothClient::setCommandRunnerForTest(const CommandRunner &runner) {
  m_runner = runner;
}

int BluetoothClient::bluetoothNodeIdFromPwDump(const QByteArray &output,
                                                const QString &address) {
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(output, &error);
  if (error.error != QJsonParseError::NoError || !doc.isArray()) {
    return -1;
  }
  const QJsonArray nodes = doc.array();
  for (const QJsonValue &value : nodes) {
    const QJsonObject node = value.toObject();
    const QJsonObject props = node.value(QStringLiteral("info"))
                                  .toObject()
                                  .value(QStringLiteral("props"))
                                  .toObject();
    if (props.value(QStringLiteral("api.bluez5.address")).toString() ==
        address) {
      return node.value(QStringLiteral("id")).toInt(-1);
    }
  }
  return -1;
}

void BluetoothClient::bluezObjectAddedForTest(const QString &objectPath,
                                              const QString &interface,
                                              const QVariantMap &props) {
  applyInterfaceAdded(objectPath, interface, props);
}

void BluetoothClient::bluezObjectRemovedForTest(const QString &objectPath,
                                                const QString &interface) {
  applyInterfaceRemoved(objectPath, interface);
}

void BluetoothClient::bluezPropertyChangedForTest(
    const QString &objectPath, const QString &interface,
    const QVariantMap &changedProps, const QStringList &invalidated) {
  applyPropertiesChanged(objectPath, interface, changedProps, invalidated);
}

void BluetoothClient::setConnectedDeviceNameForTest(const QString &name) {
  m_devices.clear();
  m_connectedOrder.clear();
  m_playerOwners.clear();
  m_takeoverDevicePath.clear();
  setTakeoverPending(false);
  updateTakeoverIncoming();
  resetAvrcp();
  if (!name.isEmpty()) {
    DeviceState d;
    d.path = QStringLiteral("/org/bluez/hci0/dev_TEST");
    d.name = name;
    d.alias = name;
    d.connected = true;
    m_devices.insert(d.path, d);
    m_connectedOrder.append(d.path);
  }
  recalculate();
}

void BluetoothClient::subscribeProperties(const QString &path,
                                           const QString &interface) {
  if (m_propertySubscribers.contains(path)) {
    return;
  }
  auto *subscriber = new BlueZPropsSubscriber(
      [this, path, interface](const QString &iface, const QVariantMap &changed,
                              const QStringList &invalidated) {
        if (iface == interface) {
          applyPropertiesChanged(path, iface, changed, invalidated);
        }
      });
  subscriber->setParent(this);
  if (m_bus.connect(kBlueZService, path, kPropertiesInterface,
                    kPropertiesSignal, subscriber,
                    SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {
    m_propertySubscribers.insert(path, subscriber);
  } else {
    subscriber->deleteLater();
  }
}

void BluetoothClient::unsubscribeProperties(const QString &path) {
  QObject *subscriber = m_propertySubscribers.take(path);
  if (!subscriber) {
    return;
  }
  m_bus.disconnect(kBlueZService, path, kPropertiesInterface, kPropertiesSignal,
                   subscriber,
                   SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
  subscriber->deleteLater();
}

void BluetoothClient::onInterfacesAdded(
    const QDBusObjectPath &path, const QMap<QString, QVariantMap> &interfaces) {
  for (auto it = interfaces.cbegin(); it != interfaces.cend(); ++it) {
    applyInterfaceAdded(path.path(), it.key(), it.value());
  }
}

void BluetoothClient::onInterfacesRemoved(const QDBusObjectPath &path,
                                           const QStringList &interfaces) {
  for (const QString &iface : interfaces) {
    applyInterfaceRemoved(path.path(), iface);
  }
}

void BluetoothClient::applyInterfaceAdded(const QString &path,
                                          const QString &interface,
                                          const QVariantMap &props) {
  if (interface == kDeviceInterface) {
    onDeviceAdded(path, props);
  } else if (interface == kMediaPlayerInterface) {
    onPlayerAdded(path, props);
  } else if (interface == kAdapterInterface) {
    onAdapterAdded(path, props);
  }
}

void BluetoothClient::applyInterfaceRemoved(const QString &path,
                                            const QString &interface) {
  if (interface == kMediaPlayerInterface) {
    onPlayerRemoved(path);
  } else if (interface == kDeviceInterface) {
    onDeviceRemoved(path);
  } else if (interface == kAdapterInterface && m_adapterPath == path) {
    unsubscribeProperties(path);
    m_adapterPath.clear();
    setAdapterPowered(false);
    setAdapterDiscoverable(false);
    setAdapterPairable(false);
  }
}

void BluetoothClient::applyPropertiesChanged(
    const QString &path, const QString &interface, const QVariantMap &props,
    const QStringList &invalidated) {
  if (props.isEmpty() && invalidated.isEmpty()) {
    return;
  }
  if (interface == kDeviceInterface) {
    onDevicePropsChanged(path, props);
  } else if (interface == kMediaPlayerInterface) {
    const QString devicePath = m_playerOwners.value(path);
    if (devicePath.isEmpty() || !m_devices.contains(devicePath) ||
        m_devices.value(devicePath).playerPath != path) {
      return;
    }
    DeviceState device = m_devices.value(devicePath);
    for (const QString &key : invalidated) {
      device.playerProperties.remove(key);
    }
    for (auto it = props.cbegin(); it != props.cend(); ++it) {
      device.playerProperties.insert(it.key(), unwrapDbusVariant(it.value()));
    }
    m_devices.insert(devicePath, device);
    if (devicePath == m_activeDevicePath) {
      ++m_playerFetchGeneration;
      resetAvrcp();
      applyPlayerProps(device.playerProperties);
    }
  } else if (interface == kAdapterInterface) {
    onAdapterPropsChanged(path, props);
  }
}

void BluetoothClient::onDeviceAdded(const QString &path,
                                     const QVariantMap &props) {
  if (m_devices.contains(path)) {
    onDevicePropsChanged(path, props);
    return;
  }
  DeviceState d;
  d.path = path;
  d.address = unwrapDbusVariant(props.value(kAddressProp)).toString();
  d.name = unwrapDbusVariant(props.value(kNameProp)).toString();
  d.alias = unwrapDbusVariant(props.value(kAliasProp)).toString();
  d.connected = unwrapDbusVariant(props.value(kConnectedProp)).toBool();
  m_devices.insert(path, d);
  if (d.connected) {
    m_connectedOrder.append(path);
    assertDeviceMute(d.address); // re-assert this device's mute against intent
  }
  if (m_adapterPath.isEmpty()) {
    const QString adapter = unwrapDbusVariant(props.value(kAdapterProp))
                                .value<QDBusObjectPath>().path();
    if (!adapter.isEmpty()) {
      m_adapterPath = adapter;
    }
  }
  subscribeProperties(path, kDeviceInterface);
  recalculate();
  for (auto it = m_deferredPlayers.begin(); it != m_deferredPlayers.end();) {
    if (it.key().startsWith(path + QLatin1Char('/'))) {
      const QString playerPath = it.key();
      const QVariantMap playerProps = it.value();
      it = m_deferredPlayers.erase(it);
      onPlayerAdded(playerPath, playerProps);
    } else {
      ++it;
    }
  }
}

void BluetoothClient::onDevicePropsChanged(const QString &path,
                                           const QVariantMap &props) {
  if (!m_devices.contains(path)) {
    return;
  }
  DeviceState device = m_devices.value(path);
  const QString oldName = deviceDisplayName(device);
  if (props.contains(kAddressProp)) {
    device.address = unwrapDbusVariant(props.value(kAddressProp)).toString();
  }
  if (props.contains(kNameProp)) {
    device.name = unwrapDbusVariant(props.value(kNameProp)).toString();
  }
  if (props.contains(kAliasProp)) {
    device.alias = unwrapDbusVariant(props.value(kAliasProp)).toString();
  }
  if (props.contains(kConnectedProp)) {
    const bool wasConnected = device.connected;
    const bool connected = unwrapDbusVariant(props.value(kConnectedProp)).toBool();
    if (connected != wasConnected) {
      device.connected = connected;
      if (connected) {
        if (!m_connectedOrder.contains(path)) {
          m_connectedOrder.append(path);
        }
      } else {
        m_connectedOrder.removeAll(path);
        device.playerProperties.clear();
      }
    }
  }
  if (m_adapterPath.isEmpty() && props.contains(kAdapterProp)) {
    m_adapterPath = unwrapDbusVariant(props.value(kAdapterProp))
                        .value<QDBusObjectPath>().path();
  }
  m_devices.insert(path, device);
  if (device.connected) {
    assertDeviceMute(device.address);
  }

  const bool nameChanged = deviceDisplayName(device) != oldName;
  const QString activeBefore = m_activeDevicePath;
  recalculate();
  // If the active device just switched, setActiveDevice already emitted.
  if (path == m_activeDevicePath && nameChanged &&
      m_activeDevicePath == activeBefore) {
    emit connectedDeviceNameChanged();
  }
}

void BluetoothClient::onDeviceRemoved(const QString &path) {
  unsubscribeProperties(path);
  for (auto it = m_deferredPlayers.begin(); it != m_deferredPlayers.end();) {
    if (it.key().startsWith(path + QLatin1Char('/'))) {
      it = m_deferredPlayers.erase(it);
    } else {
      ++it;
    }
  }
  m_devices.remove(path);
  m_connectedOrder.removeAll(path);
  for (auto it = m_playerOwners.begin(); it != m_playerOwners.end();) {
    if (it.value() == path) {
      unsubscribeProperties(it.key());
      it = m_playerOwners.erase(it);
    } else {
      ++it;
    }
  }
  if (m_takeoverDevicePath == path) {
    m_takeoverDevicePath.clear();
  }
  recalculate();
}

void BluetoothClient::onPlayerAdded(const QString &path,
                                     const QVariantMap &props) {
  const QString devicePath = parentDeviceOf(path);
  if (devicePath.isEmpty() || !m_devices.contains(devicePath)) {
    m_deferredPlayers.insert(path, props);
    return;
  }
  m_playerOwners.insert(path, devicePath);
  DeviceState device = m_devices.value(devicePath);
  if (!device.playerPath.isEmpty() && device.playerPath != path) {
    unsubscribeProperties(device.playerPath);
    m_playerOwners.remove(device.playerPath);
  }
  device.playerPath = path;
  device.playerProperties = props;
  m_devices.insert(devicePath, device);
  subscribeProperties(path, kMediaPlayerInterface);
  if (devicePath == m_activeDevicePath) {
    ++m_playerFetchGeneration;
    resetAvrcp();
    applyPlayerProps(device.playerProperties);
    refreshActivePlayer();
  }
}

void BluetoothClient::onPlayerRemoved(const QString &path) {
  m_deferredPlayers.remove(path);
  unsubscribeProperties(path);
  m_playerOwners.remove(path);
  bool activePlayerRemoved = false;
  for (auto it = m_devices.begin(); it != m_devices.end(); ++it) {
    if (it->playerPath == path) {
      activePlayerRemoved = it.key() == m_activeDevicePath;
      it->playerPath.clear();
      it->playerProperties.clear();
      ++m_playerFetchGeneration;
      break;
    }
  }
  if (activePlayerRemoved) {
    resetAvrcp();
  }
}

void BluetoothClient::onAdapterAdded(const QString &path, const QVariantMap &props) {
  if (m_adapterPath.isEmpty()) {
    m_adapterPath = path;
  }
  subscribeProperties(path, kAdapterInterface);
  applyAdapterProps(props);
}

void BluetoothClient::onAdapterPropsChanged(const QString &,
                                           const QVariantMap &props) {
  // Base Powered/Discoverable/Pairable policy is NixOS-owned; the app observes
  // the adapter (never building a discoverable toggle) and only re-asserts
  // Discoverable=true on entering BluetoothWaiting (ensureDiscoverable).
  applyAdapterProps(props);
}

void BluetoothClient::applyPlayerProps(const QVariantMap &props) {
  if (props.contains(kStatusProp)) {
    setPlayerStatusPublished(true);
    setPlayerPlaying(unwrapDbusVariant(props.value(kStatusProp)).toString() ==
                      QStringLiteral("playing"));
  }
  if (props.contains(kTrackProp)) {
    const QVariantMap track = decodedProperties(props.value(kTrackProp));
    if (track.isEmpty()) {
      setPlayerTrackPublished(false);
      setPlayerMetadata(QString(), QString(), QString(), 0);
    } else {
      setPlayerTrackPublished(true);
      setPlayerMetadata(track.value(QStringLiteral("Title")).toString(),
                        track.value(QStringLiteral("Artist")).toString(),
                        track.value(QStringLiteral("Album")).toString(),
                        qint64(track.value(kDurationKey).toUInt()));
    }
  }
  if (props.contains(kPositionProp)) {
    setPlayerPositionPublished(true);
    setPlayerPosition(qint64(unwrapDbusVariant(props.value(kPositionProp)).toUInt()));
  }
}

void BluetoothClient::applyAdapterProps(const QVariantMap &props) {
  if (props.contains(kPoweredProp)) {
    setAdapterPowered(unwrapDbusVariant(props.value(kPoweredProp)).toBool());
  }
  if (props.contains(kDiscoverableProp)) {
    setAdapterDiscoverable(unwrapDbusVariant(props.value(kDiscoverableProp)).toBool());
  }
  if (props.contains(kPairableProp)) {
    setAdapterPairable(unwrapDbusVariant(props.value(kPairableProp)).toBool());
  }
}

void BluetoothClient::resetAvrcp() {
  setPlayerStatusPublished(false);
  setPlayerTrackPublished(false);
  setPlayerPositionPublished(false);
  setPlayerPlaying(false);
  setPlayerMetadata(QString(), QString(), QString(), 0);
  setPlayerPosition(0);
}

QString BluetoothClient::parentDeviceOf(const QString &objectPath) const {
  const int slash = objectPath.lastIndexOf(QLatin1Char('/'));
  if (slash <= 0) {
    return QString();
  }
  const QString candidate = objectPath.left(slash);
  return m_devices.contains(candidate) ? candidate : QString();
}

void BluetoothClient::recalculate() {
  if (m_takeoverResolving && !m_devices.value(m_takeoverTargetPath).connected) {
    finishTakeoverAttempt();
  }

  QStringList connectedPaths;
  for (const QString &path : m_connectedOrder) {
    if (m_devices.value(path).connected) {
      connectedPaths.append(path);
    }
  }

  if (m_activeDevicePath.isEmpty() || !connectedPaths.contains(m_activeDevicePath)) {
    setActiveDevice(connectedPaths.isEmpty() ? QString()
                                             : connectedPaths.first());
  }

  if (connectedPaths.size() >= 2) {
    if (!m_takeoverPending) {
      setTakeoverError(QString());
      setTakeoverPending(true);
    }
    m_takeoverDevicePath = connectedPaths.last();
  } else {
    setTakeoverError(QString());
    if (m_takeoverPending) {
      setTakeoverPending(false);
    }
    m_takeoverDevicePath.clear();
  }
  updateTakeoverIncoming();
}

void BluetoothClient::setActiveDevice(const QString &path) {
  if (m_activeDevicePath == path) {
    return;
  }
  m_activeDevicePath = path;
  ++m_playerFetchGeneration;
  resetAvrcp();
  const DeviceState device = m_devices.value(path);
  if (!device.playerPath.isEmpty()) {
    applyPlayerProps(device.playerProperties);
    refreshActivePlayer();
  }
  emit connectedDeviceNameChanged();
}

void BluetoothClient::refreshActivePlayer() {
  const QString devicePath = m_activeDevicePath;
  const QString playerPath = m_devices.value(devicePath).playerPath;
  if (playerPath.isEmpty()) {
    return;
  }
  const quint64 generation = ++m_playerFetchGeneration;
  QPointer<BluetoothClient> self(this);
  m_dbusCall(kBlueZService, playerPath, kPropertiesInterface,
             QStringLiteral("GetAll"), QVariantList{kMediaPlayerInterface},
             [self, devicePath, playerPath, generation](
                 const QVariant &reply, const QString &error) {
               if (!self || !error.isEmpty() || !reply.isValid() ||
                   generation != self->m_playerFetchGeneration ||
                   self->m_activeDevicePath != devicePath ||
                   self->m_devices.value(devicePath).playerPath != playerPath) {
                 return;
               }
               DeviceState device = self->m_devices.value(devicePath);
               device.playerProperties = decodedProperties(reply);
               self->m_devices.insert(devicePath, device);
               self->resetAvrcp();
               self->applyPlayerProps(device.playerProperties);
             });
}

void BluetoothClient::resolveTakeover(TakeoverChoice choice) {
  if (!m_takeoverPending || m_takeoverResolving) {
    return;
  }
  const QString target = choice == KeepCurrent ? m_takeoverDevicePath
                                               : m_activeDevicePath;
  if (target.isEmpty() || !m_devices.value(target).connected) {
    recalculate();
    return;
  }

  m_takeoverTargetPath = target;
  setTakeoverError(QString());
  setTakeoverResolving(true);
  const quint64 attempt = ++m_takeoverAttempt;
  m_takeoverTimer.start();

  QPointer<BluetoothClient> self(this);
  m_dbusCall(kBlueZService, target, kDeviceInterface,
             QStringLiteral("Disconnect"), QVariantList(),
             [self, target, attempt](const QVariant &, const QString &error) {
               if (!self || !self->m_takeoverResolving ||
                   self->m_takeoverAttempt != attempt ||
                   self->m_takeoverTargetPath != target) {
                 return;
               }
               if (!error.isEmpty()) {
                 self->finishTakeoverAttempt(QStringLiteral(
                                                 "Bluetooth disconnect failed — %1")
                                                 .arg(error));
               }
               // A successful method reply is not proof of disconnection;
               // wait for Device1.Connected=false or InterfacesRemoved.
             });
}

void BluetoothClient::finishTakeoverAttempt(const QString &error) {
  m_takeoverTimer.stop();
  m_takeoverTargetPath.clear();
  setTakeoverResolving(false);
  setTakeoverError(error);
}

void BluetoothClient::setTakeoverResolving(bool resolving) {
  if (m_takeoverResolving == resolving) {
    return;
  }
  m_takeoverResolving = resolving;
  emit takeoverResolvingChanged();
}

void BluetoothClient::setTakeoverError(const QString &error) {
  if (m_takeoverError == error) {
    return;
  }
  m_takeoverError = error;
  emit takeoverErrorChanged();
}

void BluetoothClient::ensureDiscoverable() {
  if (m_adapterPath.isEmpty()) {
    return;
  }
  m_dbusCall(kBlueZService, m_adapterPath, kPropertiesInterface,
              QStringLiteral("Set"),
              QVariantList{kAdapterInterface, kDiscoverableProp,
                           QVariant::fromValue(QDBusVariant(true))},
             [](const QVariant &, const QString &) {});
}

void BluetoothClient::play() {
  const DeviceState &device = m_devices.value(m_activeDevicePath);
  if (device.playerPath.isEmpty()) {
    return;
  }
  m_dbusCall(kBlueZService, device.playerPath, kMediaPlayerInterface,
             QStringLiteral("Play"), QVariantList(),
             [](const QVariant &, const QString &) {});
}

void BluetoothClient::pause() {
  const DeviceState &device = m_devices.value(m_activeDevicePath);
  if (device.playerPath.isEmpty()) {
    return;
  }
  m_dbusCall(kBlueZService, device.playerPath, kMediaPlayerInterface,
             QStringLiteral("Pause"), QVariantList(),
             [](const QVariant &, const QString &) {});
}

void BluetoothClient::next() {
  const DeviceState &device = m_devices.value(m_activeDevicePath);
  if (device.playerPath.isEmpty()) {
    return;
  }
  m_dbusCall(kBlueZService, device.playerPath, kMediaPlayerInterface,
             QStringLiteral("Next"), QVariantList(),
             [](const QVariant &, const QString &) {});
}

void BluetoothClient::previous() {
  const DeviceState &device = m_devices.value(m_activeDevicePath);
  if (device.playerPath.isEmpty()) {
    return;
  }
  m_dbusCall(kBlueZService, device.playerPath, kMediaPlayerInterface,
             QStringLiteral("Previous"), QVariantList(),
             [](const QVariant &, const QString &) {});
}

void BluetoothClient::setMuted(bool muted) {
  if (m_muted != muted) {
    m_muted = muted;
    emit mutedChanged();
  }
  if (m_muted) {
    // Spotify audible: hard-mute EVERY connected A2DP node (whichever of them
    // is genuinely streaming) — ADR 0008, "mute all".
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
      if (it->connected && !it->address.isEmpty()) {
        setNodeMuted(it->address, true);
      }
    }
  } else {
    // Bluetooth audible: unmute only the ACTIVE device (one phone plays, Q12).
    if (m_activeDevicePath.isEmpty()) {
      return;
    }
    const QString address = m_devices.value(m_activeDevicePath).address;
    if (!address.isEmpty()) {
      setNodeMuted(address, false);
    }
  }
}

void BluetoothClient::setNodeMuted(const QString &address, bool muted) {
  QPointer<BluetoothClient> self(this);
  m_runner(QStringList{QStringLiteral("pw-dump")},
           [self, muted, address](const QByteArray &output) {
             if (!self) {
               return;
             }
             const int nodeId = bluetoothNodeIdFromPwDump(output, address);
             if (nodeId < 0) {
               return;
             }
             self->m_runner(QStringList{QStringLiteral("wpctl"),
                                        QStringLiteral("set-mute"),
                                        QString::number(nodeId),
                                        muted ? QStringLiteral("1")
                                              : QStringLiteral("0")},
                            [](const QByteArray &) {});
           });
}

void BluetoothClient::assertDeviceMute(const QString &address) {
  if (!address.isEmpty()) {
    setNodeMuted(address, m_muted);
  }
}

void BluetoothClient::pauseAll() {
  for (auto it = m_playerOwners.cbegin(); it != m_playerOwners.cend(); ++it) {
    m_dbusCall(kBlueZService, it.key(), kMediaPlayerInterface,
               QStringLiteral("Pause"), QVariantList(),
               [](const QVariant &, const QString &) {});
  }
}

void BluetoothClient::updateTakeoverIncoming() {
  QString name;
  if (!m_takeoverDevicePath.isEmpty()) {
    name = deviceDisplayName(m_devices.value(m_takeoverDevicePath));
  }
  setTakeoverIncomingName(name);
}

void BluetoothClient::setTakeoverIncomingName(const QString &name) {
  if (m_takeoverIncomingName == name) {
    return;
  }
  m_takeoverIncomingName = name;
  emit takeoverIncomingNameChanged();
}

void BluetoothClient::setTakeoverPending(bool pending) {
  if (m_takeoverPending == pending) {
    return;
  }
  m_takeoverPending = pending;
  emit takeoverPendingChanged();
}

void BluetoothClient::setAdapterPowered(bool powered) {
  if (m_adapterPowered == powered) {
    return;
  }
  m_adapterPowered = powered;
  emit adapterPoweredChanged();
}

void BluetoothClient::setAdapterDiscoverable(bool discoverable) {
  if (m_adapterDiscoverable == discoverable) {
    return;
  }
  m_adapterDiscoverable = discoverable;
  emit adapterDiscoverableChanged();
}

void BluetoothClient::setAdapterPairable(bool pairable) {
  if (m_adapterPairable == pairable) {
    return;
  }
  m_adapterPairable = pairable;
  emit adapterPairableChanged();
}

void BluetoothClient::setPlayerStatusPublished(bool published) {
  if (m_statusPublished == published) {
    return;
  }
  m_statusPublished = published;
  emit statusPublishedChanged();
}

void BluetoothClient::setPlayerTrackPublished(bool published) {
  if (m_trackPublished == published) {
    return;
  }
  m_trackPublished = published;
  emit trackPublishedChanged();
}

void BluetoothClient::setPlayerPositionPublished(bool published) {
  if (m_positionPublished == published) {
    return;
  }
  m_positionPublished = published;
  emit positionPublishedChanged();
}

void BluetoothClient::setPlayerMetadata(const QString &title,
                                        const QString &artist,
                                        const QString &album, qint64 duration) {
  if (m_trackTitle == title && m_trackArtist == artist &&
      m_trackAlbum == album && m_duration == duration) {
    return;
  }
  m_trackTitle = title;
  m_trackArtist = artist;
  m_trackAlbum = album;
  m_duration = duration;
  emit trackMetadataChanged();
}

void BluetoothClient::setPlayerPlaying(bool playing) {
  if (m_isBluetoothPlaying == playing) {
    return;
  }
  m_isBluetoothPlaying = playing;
  emit statusChanged();
}

void BluetoothClient::setPlayerPosition(qint64 ms) {
  if (m_position == ms) {
    return;
  }
  m_position = ms;
  emit positionChanged();
}
#include "BluetoothClient.moc"
