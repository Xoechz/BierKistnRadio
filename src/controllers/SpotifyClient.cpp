#include "SpotifyClient.h"
#include "ControllerError.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QTimer>

namespace {
// spotifyd 0.4.x always owns "rs.spotifyd.instance<PID>" (the D-Bus name that
// exposes the always-on rs.spotifyd.Controls interface at
// /rs/spotifyd/Controls) as soon as it is running. It only owns a *session*
// MPRIS name once an active Spotify session connects, and releases it again on
// disconnection. Both names carry the PID and change on restart, so we
// watch/scan for both uses the same rescan machinery.
const QString kMprisSessionPrefix =
    QStringLiteral("org.mpris.MediaPlayer2.spotifyd.");
const QString kSpotifydDaemonPrefix = QStringLiteral("rs.spotifyd.instance");

const QString kPlayerPath = QStringLiteral("/org/mpris/MediaPlayer2");

const QString kPropertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");
const QString kPlayerInterface =
    QStringLiteral("org.mpris.MediaPlayer2.Player");

const QString kPropertiesChanged = QStringLiteral("PropertiesChanged");

const QString kPlaybackStatus = QStringLiteral("PlaybackStatus");
const QString kMetadata = QStringLiteral("Metadata");
const QString kPosition = QStringLiteral("Position");

const QString kTitleKey = QStringLiteral("xesam:title");
const QString kArtistKey = QStringLiteral("xesam:artist");
const QString kAlbumKey = QStringLiteral("xesam:album");
const QString kArtUrlKey = QStringLiteral("mpris:artUrl");
const QString kLengthKey = QStringLiteral("mpris:length");
const QString kTrackIdKey = QStringLiteral("mpris:trackid");
const QString kNoTrackTrackId =
    QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");

const QString kPlaying = QStringLiteral("Playing");
const QString kStopped = QStringLiteral("Stopped");
} // namespace

SpotifyClient::SpotifyClient(QObject *parent)
    : SpotifyClient(QDBusConnection::sessionBus(), parent) {}

SpotifyClient::SpotifyClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent), m_bus(bus) {
  // QDBusServiceWatcher only matches exact bus names, but spotifyd's MPRIS2
  // name carries its PID and changes on restart. Watch every prefixed name we
  // discover, and re-list periodically to catch new PIDs' replacements.
  if (m_bus.isConnected()) {
    m_watcher =
        new QDBusServiceWatcher(QString(), m_bus,
                                QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            &SpotifyClient::onServiceOwnerChanged);
  }

  m_rescanTimer.setInterval(5000);
  connect(&m_rescanTimer, &QTimer::timeout, this,
          &SpotifyClient::discoverServices);
  m_rescanTimer.start();

  QTimer::singleShot(0, this, &SpotifyClient::discoverServices);
}

QString SpotifyClient::title() const { return m_title; }
QString SpotifyClient::artist() const { return m_artist; }

QString SpotifyClient::firstArtist() const { return m_firstArtist; }
QString SpotifyClient::album() const { return m_album; }
QString SpotifyClient::artUrl() const { return m_artUrl; }
qint64 SpotifyClient::position() const { return m_position; }
qint64 SpotifyClient::duration() const { return m_duration; }
bool SpotifyClient::isSpotifyPlaying() const { return m_isSpotifyPlaying; }
QString SpotifyClient::errorMessage() const { return m_errorMessage; }
bool SpotifyClient::hasTrack() const { return m_hasTrack; }
bool SpotifyClient::isAvailable() const {
  return !m_mprisService.isEmpty() || m_daemonPresent;
}

void SpotifyClient::play() {
  sendPlayerCommand(QStringLiteral("Play"));
}

void SpotifyClient::pause() {
  sendPlayerCommand(QStringLiteral("Pause"));
}

void SpotifyClient::next() {
  sendPlayerCommand(QStringLiteral("Next"));
}

void SpotifyClient::previous() {
  sendPlayerCommand(QStringLiteral("Previous"));
}

void SpotifyClient::seek(qint64 positionMs) {
  if (m_mprisService.isEmpty() || m_trackId.path().isEmpty()) {
    return;
  }
  sendPlayerCommand(QStringLiteral("SetPosition"),
                    {QVariant::fromValue(m_trackId), positionMs * 1000});
}

void SpotifyClient::setError(const QString &error) {
  if (m_errorMessage == error) {
    return;
  }
  m_errorMessage = error;
  emit errorMessageChanged();
}

void SpotifyClient::sendPlayerCommand(const QString &method,
                                      const QVariantList &args) {
  // No MPRIS session before a phone selects this speaker is normal.
  if (m_mprisService.isEmpty()) {
    return;
  }
  const quint64 generation = ++m_commandGeneration;
  setError(QString());
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPlayerInterface, method);
  msg.setArguments(args);
  auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, generation, method]() {
            if (generation == m_commandGeneration && watcher->isError()) {
              setError(controllerErrorText(
                  QStringLiteral("Spotify %1").arg(method),
                  watcher->error().name() + QStringLiteral(": ") +
                      watcher->error().message()));
            }
            watcher->deleteLater();
          });
}

void SpotifyClient::setAvailableForTest(bool available) {
  m_mprisService = available
                       ? QStringLiteral("org.mpris.MediaPlayer2.spotifyd.test")
                       : QString();
  setAvailable(available);
}

void SpotifyClient::setHasTrackForTest(bool hasTrack) {
  setTrackPresence(hasTrack);
}

void SpotifyClient::setMetadataForTest(const QVariantMap &metadata) {
  updateFromMetadata(metadata);
}

void SpotifyClient::discoverServices() {
  auto *bus = m_bus.interface();
  if (!bus) {
    return;
  }
  QDBusReply<QStringList> reply = bus->registeredServiceNames();
  if (!reply.isValid()) {
    return;
  }

  bool daemonSeen = false;
  for (const QString &name : reply.value()) {
    if (name.startsWith(kMprisSessionPrefix)) {
      m_mprisService = name;
      if (m_watcher) {
        m_watcher->addWatchedService(name);
      }
      // Bind the PropertiesChanged match to the emitter's *unique* sender so
      // Qt never has to resolve a well-known name at add-match time (that
      // resolution failure is what logs "Could not connect
      // org.freedesktop.DBus.Properties to onMprisPropertiesChanged"). A
      // ':1.x' sender is already "known", so connect() succeeds.
      const QString owner = bus->serviceOwner(name).value();
      if (owner != m_subscribedName) {
        unsubscribeFromMpris();
        m_subscribedName = owner.isEmpty() ? name : owner;
        subscribeToMpris();
      }
      // Safety net: even if a signal subscription is ever late/lost, re-pull
      // the full state each poll so metadata/position never go stale.
      fetchInitialMprisState();
      setAvailable(true);
    } else if (name.startsWith(kSpotifydDaemonPrefix)) {
      daemonSeen = true;
      if (m_watcher) {
        m_watcher->addWatchedService(name);
      }
    }
  }
  setDaemonPresent(daemonSeen);
}

void SpotifyClient::onServiceOwnerChanged(const QString &name,
                                          const QString &oldOwner,
                                          const QString &newOwner) {
  Q_UNUSED(oldOwner);
  if (!name.startsWith(kMprisSessionPrefix) &&
      !name.startsWith(kSpotifydDaemonPrefix)) {
    return;
  }

  if (name.startsWith(kSpotifydDaemonPrefix)) {
    // Daemon name: present means spotifyd is running (waitable); lost means
    // the daemon is gone entirely.
    if (!newOwner.isEmpty()) {
      setDaemonPresent(true);
      setAvailable(true);
    } else {
      setDaemonPresent(false);
      if (m_mprisService.isEmpty()) {
        setAvailable(false);
      }
    }
    return;
  }

  // Session MPRIS name. newOwner is the name's *unique* owner — attach the
  // PropertiesChanged match to that sender (see comment in discoverServices).
  if (!newOwner.isEmpty()) {
    m_mprisService = name;
    if (m_watcher) {
      m_watcher->addWatchedService(name);
    }
    if (newOwner != m_subscribedName) {
      unsubscribeFromMpris();
      m_subscribedName = newOwner;
      subscribeToMpris();
    }
    fetchInitialMprisState();
    setAvailable(true);
  } else {
    m_mprisService.clear();
    unsubscribeFromMpris();
    m_subscribedName.clear();
    m_trackId = QDBusObjectPath();
    setTrackPresence(false);
    if (!m_daemonPresent) {
      setAvailable(false);
    }
  }
}

void SpotifyClient::subscribeToMpris() {
  m_bus.connect(
      m_subscribedName, kPlayerPath, kPropertiesInterface, kPropertiesChanged,
      this, SLOT(onMprisPropertiesChanged(QString, QVariantMap, QStringList)));
}

void SpotifyClient::unsubscribeFromMpris() {
  m_bus.disconnect(
      m_subscribedName, kPlayerPath, kPropertiesInterface, kPropertiesChanged,
      this, SLOT(onMprisPropertiesChanged(QString, QVariantMap, QStringList)));
}

void SpotifyClient::fetchInitialMprisState() {
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPropertiesInterface, "GetAll");
  msg << kPlayerInterface;
  QDBusReply<QVariantMap> reply = m_bus.call(msg);

  if (!reply.isValid()) {
    return;
  }

  QVariantMap props = reply.value();
  if (props.contains(kPlaybackStatus)) {
    updatePlaybackStatus(props[kPlaybackStatus].toString());
  }
  if (props.contains(kMetadata)) {
    QDBusArgument arg = props[kMetadata].value<QDBusArgument>();
    updateFromMetadata(qdbus_cast<QVariantMap>(arg));
  }
  if (props.contains(kPosition)) {
    m_position = props[kPosition].toLongLong() / 1000;
    emit positionChanged();
  }
}

void SpotifyClient::onMprisPropertiesChanged(const QString &interface,
                                             const QVariantMap &changed,
                                             const QStringList &invalidated) {
  Q_UNUSED(invalidated);

  if (interface != kPlayerInterface) {
    return;
  }

  if (changed.contains(kPlaybackStatus)) {
    updatePlaybackStatus(changed[kPlaybackStatus].toString());
  }

  if (changed.contains(kMetadata)) {
    QDBusArgument arg = changed[kMetadata].value<QDBusArgument>();
    updateFromMetadata(qdbus_cast<QVariantMap>(arg));
  }

  if (changed.contains(kPosition)) {
    m_position = changed[kPosition].toLongLong() / 1000;
    emit positionChanged();
  }
}

void SpotifyClient::updateFromMetadata(const QVariantMap &metadata) {
  if (metadata.contains(kTitleKey)) {
    QString v = metadata[kTitleKey].toString();
    if (m_title != v) {
      m_title = v;
      emit titleChanged();

      // if the track changed, reset the position to 0. If a position update
      // comes in later, it will overwrite this.
      m_position = 0;
      emit positionChanged();
    }
  }

  if (metadata.contains(kArtistKey)) {
    QString v;
    QString first;
    if (metadata[kArtistKey].canConvert<QStringList>()) {
      const QStringList artists = metadata[kArtistKey].toStringList();
      v = artists.join(", ");
      first = artists.value(0);
    } else {
      v = metadata[kArtistKey].toString();
      first = v;
    }
    if (m_artist != v || m_firstArtist != first) {
      m_artist = v;
      m_firstArtist = first;
      emit artistChanged();
    }
  }
  if (metadata.contains(kAlbumKey)) {
    QString v = metadata[kAlbumKey].toString();
    if (m_album != v) {
      m_album = v;
      emit albumChanged();
    }
  }
  if (metadata.contains(kArtUrlKey)) {
    QString v = metadata[kArtUrlKey].toString();
    if (m_artUrl != v) {
      m_artUrl = v;
      emit artUrlChanged();
    }
  }
  if (metadata.contains(kLengthKey)) {
    qint64 v = metadata[kLengthKey].toLongLong() / 1000;
    if (m_duration != v) {
      m_duration = v;
      emit durationChanged();
    }
  }
  if (metadata.contains(kTrackIdKey)) {
    m_trackId = metadata[kTrackIdKey].value<QDBusObjectPath>();
    setTrackPresence(!m_trackId.path().isEmpty() &&
                     m_trackId.path() != kNoTrackTrackId);
  }
}

void SpotifyClient::updatePlaybackStatus(const QString &status) {
  bool playing = (status == kPlaying);
  if (m_isSpotifyPlaying != playing) {
    m_isSpotifyPlaying = playing;
    emit isSpotifyPlayingChanged();
  }
  if (status == kStopped) {
    setTrackPresence(false);
  }
}

void SpotifyClient::setTrackPresence(bool present) {
  if (m_hasTrack != present) {
    m_hasTrack = present;
    emit hasTrackChanged();
  }
}

void SpotifyClient::setAvailable(bool available) {
  if (m_available != available) {
    m_available = available;
    emit availableChanged();
  }
}

void SpotifyClient::setDaemonPresent(bool present) {
  if (m_daemonPresent == present) {
    return;
  }
  m_daemonPresent = present;
  emit availableChanged();
}
