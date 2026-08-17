#pragma once

#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <qqmlintegration.h>

class BluetoothClient : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(BluetoothClient)
  QML_UNCREATABLE("BluetoothClient is owned by PlaybackController")

  Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY
                 connectedDeviceNameChanged)
  Q_PROPERTY(
      bool takeoverPending READ takeoverPending NOTIFY takeoverPendingChanged)
  Q_PROPERTY(QString takeoverIncomingName READ takeoverIncomingName NOTIFY
                 takeoverIncomingNameChanged)
  Q_PROPERTY(bool adapterPowered READ adapterPowered NOTIFY
                 adapterPoweredChanged)
  Q_PROPERTY(bool adapterDiscoverable READ adapterDiscoverable NOTIFY
                 adapterDiscoverableChanged)
  Q_PROPERTY(bool adapterPairable READ adapterPairable NOTIFY
                 adapterPairableChanged)
  Q_PROPERTY(
      bool statusPublished READ statusPublished NOTIFY statusPublishedChanged)
  Q_PROPERTY(
      bool trackPublished READ trackPublished NOTIFY trackPublishedChanged)
  Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackMetadataChanged)
  Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY trackMetadataChanged)
  Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY trackMetadataChanged)
  Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY trackMetadataChanged)
  Q_PROPERTY(bool positionPublished READ positionPublished NOTIFY
                 positionPublishedChanged)
  Q_PROPERTY(
      bool isBluetoothPlaying READ isBluetoothPlaying NOTIFY statusChanged)
  Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)

public:
  enum TakeoverChoice { KeepCurrent, SwitchToNew };
  Q_ENUM(TakeoverChoice)

  explicit BluetoothClient(QObject *parent = nullptr);

  QString connectedDeviceName() const;
  bool takeoverPending() const;
  QString takeoverIncomingName() const;
  bool adapterPowered() const;
  bool adapterDiscoverable() const;
  bool adapterPairable() const;

  bool statusPublished() const;
  bool trackPublished() const;
  QString trackTitle() const;
  QString trackArtist() const;
  QString trackAlbum() const;
  qint64 position() const;
  qint64 duration() const;
  bool positionPublished() const;
  bool isBluetoothPlaying() const;
  bool muted() const;

  Q_INVOKABLE void resolveTakeover(TakeoverChoice choice);
  Q_INVOKABLE void ensureDiscoverable();

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void setMuted(bool muted);
  // Best-effort AVRCP Pause on every connected device's player (ADR 0008:
  // switching away from BT also pauses, not just mutes).
  Q_INVOKABLE void pauseAll();

  // BlueZ D-Bus call seam. `onFinished(reply, error)` receives the unwrapped
  // reply value (or empty on error) plus a non-empty error string on
  // D-Bus / method failure. Injectable so tests never need a live BlueZ daemon.
  using DbusCallable = std::function<void(
      const QString &service, const QString &objectPath,
      const QString &interface, const QString &method, const QVariantList &args,
      const std::function<void(const QVariant &reply, const QString &error)> &onFinished)>;
  // Shell-out seam for `pw-dump`/`wpctl` (mute node discovery). `args` is the
  // full argv including the program. `onFinished` receives stdout.
  using CommandRunner = std::function<void(
      const QStringList &args,
      const std::function<void(const QByteArray &output)> &onFinished)>;
  void setDbusCallableForTest(const DbusCallable &callable);
  void setCommandRunnerForTest(const CommandRunner &runner);

  // Pure helper: scan `pw-dump` JSON for the PipeWire node whose
  // `api.bluez5.address` equals `address`; returns its numeric id, or -1.
  static int bluetoothNodeIdFromPwDump(const QByteArray &output,
                                       const QString &address);

  // Test hooks: deliver BlueZ object-manager / PropertiesChanged events the way
  // the real D-Bus subscriptions do at runtime. `interface` is e.g.
  // "org.bluez.Device1", "org.bluez.MediaPlayer1", "org.bluez.Adapter1".
  void bluezObjectAddedForTest(const QString &objectPath,
                               const QString &interface,
                               const QVariantMap &props);
  void bluezObjectRemovedForTest(const QString &objectPath,
                                 const QString &interface);
  void bluezPropertyChangedForTest(const QString &objectPath,
                                   const QString &interface,
                                   const QVariantMap &changedProps);

  // Test hook (legacy): drives connection state the way BlueZ updates it at
  // runtime, without needing a live device. Empty name = all disconnected.
  void setConnectedDeviceNameForTest(const QString &name);

signals:
  void connectedDeviceNameChanged();
  void takeoverPendingChanged();
  void takeoverIncomingNameChanged();
  void adapterPoweredChanged();
  void adapterDiscoverableChanged();
  void adapterPairableChanged();
  void statusPublishedChanged();
  void trackPublishedChanged();
  void trackMetadataChanged();
  void positionChanged();
  void positionPublishedChanged();
  void statusChanged();
  void mutedChanged();

private:
  struct DeviceState {
    QString path;
    QString address;
    QString name;
    QString alias;
    QString playerPath;
    bool connected = false;
  };

  void applyInterfaceAdded(const QString &path, const QString &interface,
                           const QVariantMap &props);
  void applyInterfaceRemoved(const QString &path, const QString &interface);
  void applyPropertiesChanged(const QString &path, const QString &interface,
                              const QVariantMap &props);
  void subscribeProperties(const QString &path, const QString &interface);
  void onDeviceAdded(const QString &path, const QVariantMap &props);
  void onDevicePropsChanged(const QString &path, const QVariantMap &props);
  void onDeviceRemoved(const QString &path);
  void onPlayerAdded(const QString &path, const QVariantMap &props);
  void onPlayerPropsChanged(const QString &path, const QVariantMap &props);
  void onPlayerRemoved(const QString &path);
  void onAdapterAdded(const QString &path, const QVariantMap &props);
  void onAdapterPropsChanged(const QString &path, const QVariantMap &props);

  void applyPlayerProps(const QVariantMap &props);
  void applyAdapterProps(const QVariantMap &props);
  void resetAvrcp();
  QString parentDeviceOf(const QString &objectPath) const;
  void recalculate();
  void setActiveDevice(const QString &path);
  void disconnectDevice(const QString &path);
  void updateTakeoverIncoming();
  void setTakeoverPending(bool pending);
  void setTakeoverIncomingName(const QString &name);
  void setAdapterPowered(bool powered);
  void setAdapterDiscoverable(bool discoverable);
  void setAdapterPairable(bool pairable);
  void setPlayerStatusPublished(bool published);
  void setPlayerTrackPublished(bool published);
  void setPlayerPositionPublished(bool published);
  void setPlayerMetadata(const QString &title, const QString &artist,
                         const QString &album, qint64 duration);
  void setPlayerPlaying(bool playing);
  void setPlayerPosition(qint64 ms);

  // Mute single A2DP node by bluez address (pw-dump -> wpctl). `assertDeviceMute`
  // (re)applies the current intent (`m_muted`) to a freshly connected device.
  void setNodeMuted(const QString &address, bool muted);
  void assertDeviceMute(const QString &address);

private slots:
  void onInterfacesAdded(const QDBusObjectPath &path,
                         const QMap<QString, QVariantMap> &interfaces);
  void onInterfacesRemoved(const QDBusObjectPath &path,
                           const QStringList &interfaces);

private:
  DbusCallable m_dbusCall;
  CommandRunner m_runner;

  QMap<QString, DeviceState> m_devices; // Device1 path -> state
  QStringList m_connectedOrder;         // connection order (last = newest)
  QMap<QString, QString> m_playerOwners; // player path -> device path
  QString m_activeDevicePath;
  QString m_adapterPath;
  QString m_takeoverDevicePath;
  QString m_takeoverIncomingName;
  bool m_takeoverPending = false;
  bool m_adapterPowered = false;
  bool m_adapterDiscoverable = false;
  bool m_adapterPairable = false;

  bool m_statusPublished = false;
  bool m_trackPublished = false;
  bool m_positionPublished = false;
  QString m_trackTitle;
  QString m_trackArtist;
  QString m_trackAlbum;
  qint64 m_position = 0;
  qint64 m_duration = 0;
  bool m_isBluetoothPlaying = false;
  bool m_muted = false;
};