#pragma once

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusServiceWatcher>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <qqmlintegration.h>
#include "PairingAgent.h"

class BluetoothClient : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(BluetoothClient)
  QML_UNCREATABLE("BluetoothClient is owned by PlaybackController")

  Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY
                 connectedDeviceNameChanged)
  Q_PROPERTY(bool connected READ hasConnectedDevice NOTIFY
                 connectedDeviceNameChanged)
  Q_PROPERTY(
      bool takeoverPending READ takeoverPending NOTIFY takeoverPendingChanged)
  Q_PROPERTY(QString takeoverIncomingName READ takeoverIncomingName NOTIFY
                 takeoverIncomingNameChanged)
  Q_PROPERTY(bool takeoverResolving READ takeoverResolving NOTIFY
                 takeoverResolvingChanged)
  Q_PROPERTY(QString takeoverError READ takeoverError NOTIFY takeoverErrorChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
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
  Q_PROPERTY(PairingAgent *pairing READ pairing CONSTANT)

public:
  enum TakeoverChoice { KeepCurrent, SwitchToNew };
  Q_ENUM(TakeoverChoice)

  /// Construct on the real BlueZ system bus. @param parent Optional owner; no return.
  explicit BluetoothClient(QObject *parent = nullptr);
  /// Create pairing agent, dispatchers, subscriptions, takeover timer and adapter polling.
  /// @param bus BlueZ connection. @param parent Optional owner. No return value.
  explicit BluetoothClient(const QDBusConnection &bus, QObject *parent = nullptr);

  /// @return Active device's Alias/Name/Address/fallback text, or empty without selection.
  QString connectedDeviceName() const;
  /// @return Whether an active-device path is cached.
  bool hasConnectedDevice() const;
  /// @return Whether connected phones need an arbitration choice.
  bool takeoverPending() const;
  /// @return Contending phone display name, or empty without a contender.
  QString takeoverIncomingName() const;
  /// @return Whether a losing-device disconnect awaits observed resolution.
  bool takeoverResolving() const;
  /// @return Current arbitration-attempt error, or empty.
  QString takeoverError() const;
  /// @return Operation error, then first keyed background error, otherwise empty.
  QString errorMessage() const;
  /// @return Last Powered value; check adapterStateKnown before treating it as proof.
  bool adapterPowered() const;
  /// @return Last observed Discoverable flag.
  bool adapterDiscoverable() const;
  /// @return Last observed Pairable flag.
  bool adapterPairable() const;
  /// @return Whether current adapter state/observed absence is validated.
  bool adapterStateKnown() const { return m_adapterStateKnown; }
  /// @return Whether a selected adapter path exists, not a fresh hardware query.
  bool adapterAvailable() const { return !m_adapterPath.isEmpty(); }
  /// @return Cached power plus local A2DP Sink UUID; state-known is checked separately.
  bool sinkReady() const { return m_adapterPowered && m_sinkPublished; }
  /// Submit power Set, invalidate old reads, then request fresh observation. No return.
  /// @param powered Desired boolean. @param finished Empty acknowledgement/classified error.
  /// Success of finished(error) does not prove the adapter actually changed power.
  void requestAdapterPowered(bool powered,
                             const std::function<void(const QString &)> &finished);

  /// @return Whether active player published Status; not proof every control works.
  bool statusPublished() const;
  /// @return Whether active player supplied a nonempty Track map.
  bool trackPublished() const;
  /// @return Active cached title, empty after metadata reset.
  QString trackTitle() const;
  /// @return Active cached artist text.
  QString trackArtist() const;
  /// @return Active cached album text.
  QString trackAlbum() const;
  /// @return Cached position in milliseconds; publication is a separate flag.
  qint64 position() const;
  /// @return Cached duration in milliseconds; zero for absent/default duration.
  qint64 duration() const;
  /// @return Whether active player supplied Position.
  bool positionPublished() const;
  /// @return Whether active player's cached Status equals lowercase playing.
  bool isBluetoothPlaying() const;
  /// @return Supplemental mute intent, not verified silence or lifecycle shutdown.
  bool muted() const;
  /// @return Borrowed pointer to this client's owned Agent1 implementation.
  PairingAgent *pairing() const { return m_pairing; }
  /// @param enabled Source-level pairing permission; also requires adapter power.
  /// No return value; registration/rejection effects are asynchronous.
  void setPairingEnabled(bool enabled);
  /// @param enabled Permit supplemental node mute work while Bluetooth is ready.
  /// Disabling invalidates pending mute callbacks and clears only mute errors. No return.
  void setMuteEnabled(bool enabled);

  /// Request losing-phone disconnect and retain choice until observed disconnect/removal.
  /// @param choice KeepCurrent disconnects newcomer; SwitchToNew disconnects current.
  /// No return; five-second failure keeps the dialog pending for manual retry.
  Q_INVOKABLE void resolveTakeover(TakeoverChoice choice);
  /// Assert discovery once; do not discover/connect phones. No params/return; errors async.
  Q_INVOKABLE void ensureDiscoverable();

  /// Request active player's best-effort AVRCP Play. No parameters/return.
  Q_INVOKABLE void play();
  /// Request active player's best-effort AVRCP Pause. No parameters/return.
  Q_INVOKABLE void pause();
  /// Request active player's best-effort AVRCP Next. No parameters/return.
  Q_INVOKABLE void next();
  /// Request active player's best-effort AVRCP Previous. No parameters/return.
  Q_INVOKABLE void previous();
  /// @param muted True mutes connected addresses; false unmutes active address only.
  /// No return; async lookup picks each address's first matching node, never default sink.
  Q_INVOKABLE void setMuted(bool muted);
  // Best-effort AVRCP Pause on every connected device's player (ADR 0008:
  // switching away from BT also pauses, not just mutes).
  /// Pause all known player-owner entries supplementally. No parameters/return.
  /// Only permission failures are reported here; other best-effort failures are ignored.
  Q_INVOKABLE void pauseAll();

  // BlueZ D-Bus call seam. `onFinished(reply, error)` receives the unwrapped
  // reply value (or empty on error) plus a non-empty error string on
  // D-Bus / method failure. Injectable so tests never need a live BlueZ daemon.
  using DbusCallable = std::function<void(
      const QString &service, const QString &objectPath,
      const QString &interface, const QString &method, const QVariantList &args,
      const std::function<void(const QVariant &reply, const QString &error)> &onFinished)>;
  // Shell-out seam for `pw-dump`/`wpctl` (mute node discovery). `args` is the
  // full argv including the program. `onFinished` receives stdout and an
  // error message (empty on success).
  using CommandRunner = std::function<void(
      const QStringList &args,
      const std::function<void(const QByteArray &output, const QString &error)> &onFinished)>;
  /// @param callable Replacement dispatch/completion seam for subsequent calls. No return.
  void setDbusCallableForTest(const DbusCallable &callable);
  /// @param runner Replacement full-argv/stdout/error seam. No return value.
  void setCommandRunnerForTest(const CommandRunner &runner);

  // Pure helper: scan `pw-dump` JSON for the PipeWire node whose
  // `api.bluez5.address` equals `address`; returns its numeric id, or -1.
  /// @param output JSON dump bytes. @param address Exact api.bluez5.address to match.
  /// @return First matching object's numeric ID, or -1 for malformed/missing match.
  static int bluetoothNodeIdFromPwDump(const QByteArray &output,
                                       const QString &address);

  // Test hooks: deliver BlueZ object-manager / PropertiesChanged events the way
  // the real D-Bus subscriptions do at runtime. `interface` is e.g.
  // "org.bluez.Device1", "org.bluez.MediaPlayer1", "org.bluez.Adapter1".
  /// Deliver fixture data through runtime interface dispatch. No return value.
  /// @param objectPath Added path. @param interface BlueZ interface. @param props Dictionary.
  void bluezObjectAddedForTest(const QString &objectPath,
                               const QString &interface,
                               const QVariantMap &props);
  /// @param objectPath Removed path. @param interface Removed interface. No return value.
  void bluezObjectRemovedForTest(const QString &objectPath,
                                 const QString &interface);
  /// Deliver a fixture PropertiesChanged event through runtime handling. No return.
  /// @param objectPath Emitter path. @param interface Interface name.
  /// @param changedProps Dictionary. @param invalidated Keys to forget/refetch.
  void bluezPropertyChangedForTest(const QString &objectPath,
                                   const QString &interface,
                                   const QVariantMap &changedProps,
                                   const QStringList &invalidated = {});

  // Test hook (legacy): drives connection state the way BlueZ updates it at
  // runtime, without needing a live device. Empty name = all disconnected.
  /// @param name One synthetic connected name, or empty to clear tree/arbitration.
  /// No return; legacy hook replaces device/player maps rather than calling BlueZ.
  void setConnectedDeviceNameForTest(const QString &name);

signals:
  /// Notify active connection/identity changes. No parameters/return.
  void connectedDeviceNameChanged();
  /// Notify arbitration visibility. No parameters/return.
  void takeoverPendingChanged();
  /// Notify contender identity changes. No parameters/return.
  void takeoverIncomingNameChanged();
  /// Notify disconnect-attempt pending state. No parameters/return.
  void takeoverResolvingChanged();
  /// Notify arbitration failure/recovery. No parameters/return.
  void takeoverErrorChanged();
  /// Notify combined operation/background message. No parameters/return.
  void errorMessageChanged();
  /// Notify observed adapter power value. No parameters/return.
  void adapterPoweredChanged();
  /// Notify observed discovery value. No parameters/return.
  void adapterDiscoverableChanged();
  /// Notify observed pairability value. No parameters/return.
  void adapterPairableChanged();
  /// Notify validated/unknown adapter observation, even unchanged values. No params/return.
  void adapterStateChanged();
  /// Notify Status availability. No parameters/return.
  void statusPublishedChanged();
  /// Notify Track availability. No parameters/return.
  void trackPublishedChanged();
  /// Notify title/artist/album/duration group changes. No parameters/return.
  void trackMetadataChanged();
  /// Notify millisecond position changes. No parameters/return.
  void positionChanged();
  /// Notify Position availability. No parameters/return.
  void positionPublishedChanged();
  /// Notify playing predicate changes. No parameters/return.
  void statusChanged();
  /// Notify supplemental intent, not external mute acknowledgement. No params/return.
  void mutedChanged();

private:
  struct DeviceState {
    QString path;
    QString address;
    QString name;
    QString alias;
    QString playerPath;
    QVariantMap playerProperties;
    bool connected = false;
    bool paired = false;
  };

  /// Dispatch Device1/MediaPlayer1/Adapter1 additions. No return value.
  /// @param path Object path. @param interface Interface name. @param props Initial fields.
  void applyInterfaceAdded(const QString &path, const QString &interface,
                           const QVariantMap &props);
  /// @param path Removed object. @param interface Removed interface. No return value.
  /// Dispatch removal and invalidate selected adapter reads when appropriate.
  void applyInterfaceRemoved(const QString &path, const QString &interface);
  /// Apply fields/invalidations with selected-device/adapter safeguards. No return.
  /// @param path Emitter. @param interface Interface. @param props Changed fields.
  /// @param invalidated Keys to forget; adapter fields trigger reread.
  void applyPropertiesChanged(const QString &path, const QString &interface,
                              const QVariantMap &props,
                              const QStringList &invalidated = {});
  /// Create one path-aware property receiver and report match failure. No return.
  /// @param path Emitter path. @param interface Expected interface to filter.
  void subscribeProperties(const QString &path, const QString &interface);
  /// @param path Subscription to disconnect/delete and clear related errors. No return.
  void unsubscribeProperties(const QString &path);
  /// Get active player fields with device/path/generation checks. No parameters/return.
  void refreshActivePlayer();
  /// Cache/subscribe device, order connections and attach deferred players. No return.
  /// @param path Device path. @param props Initial identity/bond/connection fields.
  void onDeviceAdded(const QString &path, const QVariantMap &props);
  /// @param path Known device. @param props Changed identity/bond/connection fields.
  /// No return; updates active/contender presentation and supplemental intent.
  void onDevicePropsChanged(const QString &path, const QVariantMap &props);
  /// @param path Removed device; clear players/order and recalculate selection. No return.
  void onDeviceRemoved(const QString &path);
  /// Attach to known parent device, or defer until parent arrives. No return value.
  /// @param path Player path. @param props Player fields; active player is refreshed.
  void onPlayerAdded(const QString &path, const QVariantMap &props);
  /// @param path Removed player; clear associations and reset active AVRCP. No return.
  void onPlayerRemoved(const QString &path);
  /// Select first adapter path and apply only selected adapter data. No return value.
  /// @param path Adapter path. @param props Initial fields, invalidating older reads.
  void onAdapterAdded(const QString &path, const QVariantMap &props);
  /// @param path Adapter to filter. @param props Fields superseding old power/UUID reads.
  /// No return value; another adapter cannot overwrite selected state.
  void onAdapterPropsChanged(const QString &path, const QVariantMap &props);

  /// @param props Status/Track/Position fields; times are milliseconds. No return value.
  /// Update publication/playing/metadata; empty Track clears metadata availability.
  void applyPlayerProps(const QVariantMap &props);
  /// @param props Adapter fields; Powered must be boolean and UUIDs prove local sink.
  /// No return; sets cached state/errors and emits an observation notification.
  void applyAdapterProps(const QVariantMap &props);
  /// Clear published fields, metadata, playing and position. No parameters/return.
  void resetAvrcp();
  /// @param device Cached identity. @return Alias -> Name -> Address -> generic fallback.
  static QString deviceDisplayName(const DeviceState &device);
  /// @param objectPath Candidate player path. @return Nearest known device ancestor or empty.
  QString parentDeviceOf(const QString &objectPath) const;
  /// Maintain active device and newest contender from observed connection order.
  /// No parameters/return; losing-device disappearance finishes pending resolution.
  void recalculate();
  /// @param path New active device, or empty; invalidates player reads and resets data.
  /// No return; applies/fetches its player and notifies connection presentation.
  void setActiveDevice(const QString &path);
  /// Stop disconnect deadline/target, clear resolving and set attempt message.
  /// @param error Failure text or empty success/recovery. No return value.
  void finishTakeoverAttempt(const QString &error = QString());
  /// @param resolving Attempt-in-progress state; notify on change. No return value.
  void setTakeoverResolving(bool resolving);
  /// @param error Arbitration text or empty to clear; notify on change. No return.
  void setTakeoverError(const QString &error);
  /// @param error Primary operation text or empty; combined-message notification. No return.
  void setError(const QString &error);
  /// @param key Concern identifier. @param error Text or empty to remove. No return.
  void setBackgroundError(const QString &key, const QString &error);
  /// Send bounded AVRCP command, or no-op without player. No return value.
  /// @param playerPath Known target. @param method Control method; errors arrive async.
  void sendPlayerCommand(const QString &playerPath, const QString &method);
  /// Derive contender name from selected incoming path. No parameters/return.
  void updateTakeoverIncoming();
  /// @param pending Arbitration visibility; notify on change. No return value.
  void setTakeoverPending(bool pending);
  /// @param name Contender display name or empty; notify on change. No return.
  void setTakeoverIncomingName(const QString &name);
  /// @param powered Observed value; updates pairing gate and notification. No return.
  /// This setter does not issue a BlueZ power write.
  void setAdapterPowered(bool powered);
  /// @param discoverable Observed value; notification only, no external write. No return.
  void setAdapterDiscoverable(bool discoverable);
  /// @param pairable Observed value; notification only, no external write. No return.
  void setAdapterPairable(bool pairable);
  /// @param published Status availability; notify on change. No return value.
  void setPlayerStatusPublished(bool published);
  /// @param published Track availability; notify on change. No return value.
  void setPlayerTrackPublished(bool published);
  /// @param published Position availability; notify on change. No return value.
  void setPlayerPositionPublished(bool published);
  /// Change-notify the metadata group atomically. No return value.
  /// @param title Track title. @param artist Artist text. @param album Album text.
  /// @param duration Milliseconds, zero when unknown/default.
  void setPlayerMetadata(const QString &title, const QString &artist,
                         const QString &album, qint64 duration);
  /// @param playing Observed playing flag; notify on change. No return value.
  void setPlayerPlaying(bool playing);
  /// @param ms Observed milliseconds; notify on change. No return value.
  void setPlayerPosition(qint64 ms);

  // Mute single A2DP node by bluez address (pw-dump -> wpctl). `assertDeviceMute`
  // (re)applies the current intent (`m_muted`) to a freshly connected device.
  /// Locate first address-matched node then request wpctl mute; absent node is a no-op.
  /// @param address MAC. @param muted Desired node flag. No return; errors async.
  void setNodeMuted(const QString &address, bool muted);
  /// @param address One disconnected/replaced address, or empty for all operations.
  /// Invalidate pending mute callbacks and clear their scoped errors. No return.
  void invalidateMuteOperations(const QString &address = QString());
  /// @param address MAC to reapply current intent; empty ignored. No return value.
  void assertDeviceMute(const QString &address);
  /// Decode guarded BlueZ snapshot and attach devices/adapters/players. No params/return.
  void refreshManagedObjects();
  /// Invalidate generations and clear old daemon tree/subscriptions/state. No params/return.
  void clearBluezState();
  /// Replace/check addition/removal matches and report failures. No parameters/return.
  void subscribeObjectManager();
  /// Coalesce selected adapter GetAll, validate power and discard stale replies.
  /// No parameters/return; passive observation never issues power writes.
  void refreshAdapterState();

private slots:
  /// @param path Added object. @param interfaces Interface -> initial property dictionaries.
  /// No return value; dispatch each interface through runtime addition handling.
  void onInterfacesAdded(const QDBusObjectPath &path,
                         const QMap<QString, QVariantMap> &interfaces);
  /// @param path Removed object. @param interfaces Removed names. No return value.
  void onInterfacesRemoved(const QDBusObjectPath &path,
                           const QStringList &interfaces);

private:
  DbusCallable m_dbusCall;
  CommandRunner m_runner;
  QDBusConnection m_bus;
  PairingAgent *m_pairing = nullptr;
  bool m_pairingEnabled = false;
  QDBusServiceWatcher *m_serviceWatcher = nullptr;
  quint64 m_objectManagerGeneration = 0;
  QMap<QString, QObject *> m_propertySubscribers;
  QMap<QString, QVariantMap> m_deferredPlayers;
  quint64 m_playerFetchGeneration = 0;

  QMap<QString, DeviceState> m_devices; // Device1 path -> state
  QStringList m_connectedOrder;         // connection order (last = newest)
  QMap<QString, QString> m_playerOwners; // player path -> device path
  QString m_activeDevicePath;
  QString m_adapterPath;
  QString m_takeoverDevicePath;
  QString m_takeoverIncomingName;
  QString m_takeoverTargetPath;
  QString m_takeoverError;
  QString m_errorMessage;
  QMap<QString, QString> m_backgroundErrors;
  bool m_takeoverPending = false;
  bool m_takeoverResolving = false;
  quint64 m_takeoverAttempt = 0;
  QTimer m_takeoverTimer;
  bool m_adapterPowered = false;
  bool m_adapterDiscoverable = false;
  bool m_adapterPairable = false;
  bool m_adapterStateKnown = false;
  bool m_sinkPublished = false;
  quint64 m_adapterReadGeneration = 0;
  bool m_adapterReadPending = false;
  QTimer m_adapterPoll;

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
  bool m_muteEnabled = true;
  quint64 m_muteSequence = 0;
  QMap<QString, quint64> m_muteRequests;
};
