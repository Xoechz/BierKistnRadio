#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <qqmlintegration.h>

class SpotifyClient : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(SpotifyClient)
  QML_UNCREATABLE("SpotifyClient is owned by PlaybackController")

  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(QString artist READ artist NOTIFY artistChanged)
  Q_PROPERTY(QString album READ album NOTIFY albumChanged)
  Q_PROPERTY(QString artUrl READ artUrl NOTIFY artUrlChanged)
  Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
  Q_PROPERTY(bool isSpotifyPlaying READ isSpotifyPlaying NOTIFY
                  isSpotifyPlayingChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
  /// Construct on the real session bus. @param parent Optional owner; no return.
  explicit SpotifyClient(QObject *parent = nullptr);
  /// Observe dynamic spotifyd names, then schedule discovery and five-second rescans.
  /// @param bus Session-bus handle. @param parent Optional owner. No return value.
  explicit SpotifyClient(const QDBusConnection &bus, QObject *parent = nullptr);

  /// @return Cached track title; session loss does not clear every metadata field.
  QString title() const;
  /// @return Cached display artists, joined with comma-space when supplied as a list.
  QString artist() const;
  /// @return First reported artist, kept separately for release-date lookup.
  QString firstArtist() const;
  /// @return Cached album text.
  QString album() const;
  /// @return Cached remote artwork URL; the art cache supplies a local URL separately.
  QString artUrl() const;
  /// @return Cached position in milliseconds, converted from MPRIS microseconds.
  qint64 position() const;
  /// @return Cached duration in milliseconds, converted from metadata length.
  qint64 duration() const;
  /// @return Whether the last reported PlaybackStatus was Playing.
  bool isSpotifyPlaying() const;
  /// @return Primary operation error, then first keyed background error, or empty.
  QString errorMessage() const;

  /// @return Track-presence flag derived from track ID and Stopped status.
  bool hasTrack() const;
  /// @return Whether an MPRIS name or daemon name is cached; not lifecycle readiness.
  bool isAvailable() const;

  /// Send Play if an MPRIS session exists. No parameters/return; errors are async.
  Q_INVOKABLE void play();
  /// Send Pause if a session exists. No parameters/return; absent session is normal.
  Q_INVOKABLE void pause();
  /// Send Next if a session exists. No parameters/return; failures update error state.
  Q_INVOKABLE void next();
  /// Send Previous if a session exists. No parameters/return.
  Q_INVOKABLE void previous();
  /// Send SetPosition with current track ID and microsecond-converted position.
  /// @param positionMs Absolute milliseconds; not clamped here. No return value.
  Q_INVOKABLE void seek(qint64 positionMs);

  // Test hook: lets unit tests drive presence/availability the way the
  // MPRIS2 D-Bus subscription updates it at runtime, without a live bus.
  /// @param available Simulated presence; assigns/removes test service name. No return.
  void setAvailableForTest(bool available);
  /// @param hasTrack Simulated track presence; uses normal setter. No return value.
  void setHasTrackForTest(bool hasTrack);
  /// @param metadata MPRIS fields to apply through runtime decoding. No return value.
  void setMetadataForTest(const QVariantMap &metadata);

signals:
  /// Notify cached title changes. No parameters/return.
  void titleChanged();
  /// Notify display/first artist changes. No parameters/return.
  void artistChanged();
  /// Notify album changes. No parameters/return.
  void albumChanged();
  /// Notify remote artwork URL changes. No parameters/return.
  void artUrlChanged();
  /// Notify millisecond position updates/resets. No parameters/return.
  void positionChanged();
  /// Notify millisecond duration changes. No parameters/return.
  void durationChanged();
  /// Notify playing-status changes. No parameters/return.
  void isSpotifyPlayingChanged();
  /// Notify track-presence changes. No parameters/return.
  void hasTrackChanged();
  /// Notify availability observations; getter predicate is name-based. No params/return.
  void availableChanged();
  /// Notify combined visible error changes. No parameters/return.
  void errorMessageChanged();

private:
  QString m_title;
  QString m_artist;
  QString m_firstArtist;
  QString m_album;
  QString m_artUrl;
  qint64 m_position = 0;
  qint64 m_duration = 0;
  bool m_isSpotifyPlaying = false;
  bool m_hasTrack = false;
  bool m_available = false;
  QString m_errorMessage;
  QMap<QString, QString> m_backgroundErrors;
  quint64 m_commandGeneration = 0;
  quint64 m_stateGeneration = 0;
  bool m_stateReadPending = false;
  QString m_mprisService;
  QDBusConnection m_bus;
  QString m_subscribedName;
  bool m_mprisSubscribed = false;
  bool m_daemonPresent = false;
  QDBusObjectPath m_trackId;
  QDBusServiceWatcher *m_watcher = nullptr;
  QTimer m_rescanTimer;

  /// List names/resolve owners synchronously, attach unique sender, then read state async.
  /// No parameters/return; expected absence is not a fault.
  void discoverServices();
  /// Apply watched-name appearance/loss and invalidate obsolete state/commands.
  /// @param name Watched name. @param oldOwner Unused prior owner.
  /// @param newOwner Unique owner, or empty on disappearance. No return value.
  void onServiceOwnerChanged(const QString &name, const QString &oldOwner,
                             const QString &newOwner);
  /// Subscribe to the selected sender and report match failure. No params/return.
  void subscribeToMpris();
  /// Disconnect and invalidate pending snapshots. No parameters/return.
  void unsubscribeFromMpris();
  /// Coalesce bounded GetAll reads and apply only current service/generation replies.
  /// No parameters/return; completion updates properties/errors asynchronously.
  void fetchInitialMprisState();
  /// Apply present fields; missing keys do not clear all cached metadata.
  /// @param metadata xesam/mpris dictionary; wire times are microseconds. No return.
  void updateFromMetadata(const QVariantMap &metadata);
  /// @param status Playing/Paused/Stopped string; Stopped clears presence. No return.
  void updatePlaybackStatus(const QString &status);
  /// @param present New track-presence flag; notify only on change. No return.
  void setTrackPresence(bool present);
  /// @param available Internal notification flag, not isAvailable's predicate. No return.
  void setAvailable(bool available);
  /// @param present Cached daemon-name presence; notify on change. No return.
  void setDaemonPresent(bool present);
  /// @param error Primary operation text; empty clears. No return; combined error notifies.
  void setError(const QString &error);
  /// @param key Independent concern. @param error Text or empty to remove. No return.
  void setBackgroundError(const QString &key, const QString &error);
  /// Send one bounded command and report only the newest command generation's error.
  /// @param method Player method. @param args Typed arguments, default empty.
  /// @note No return; absence of an MPRIS session is a no-op, not an error.
  void sendPlayerCommand(const QString &method, const QVariantList &args = {});

private slots:
  // Slots referenced via QDBusConnection::connect(..., SLOT(...)). A slot needs
  // to be registered in the Q_OBJECT metaobject for SLOT() to resolve — a plain
  // private method makes QDBusConnection::connect() return false (it logged
  // "Could not connect org.freedesktop.DBus.Properties to
  // onMprisPropertiesChanged" for exactly that reason and never delivered).
  /// Supersede stale snapshots and apply changed Player status/metadata/position.
  /// @param interface Expected Player interface. @param changed Property dictionary.
  /// @param invalidated Currently unused keys. No return value.
  void onMprisPropertiesChanged(const QString &interface,
                                const QVariantMap &changed,
                                const QStringList &invalidated);
};
