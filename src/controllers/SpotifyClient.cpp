#include "SpotifyClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QTimer>

namespace {
// spotifyd 0.4.x always owns "rs.spotifyd.instance<PID>" (the D-Bus name that
// exposes the always-on rs.spotifyd.Controls interface at /rs/spotifyd/Controls)
// as soon as it is running. It only owns a *session* MPRIS name once an active
// Spotify session connects, and releases it again on disconnection. Both names
// carry the PID and change on restart, so we watch/scan for both uses the same
// rescan machinery.
const QString kMprisSessionPrefix =
    QStringLiteral("org.mpris.MediaPlayer2.spotifyd.");
const QString kSpotifydDaemonPrefix = QStringLiteral("rs.spotifyd.instance");

const QString kPlayerPath = QStringLiteral("/org/mpris/MediaPlayer2");

const QString kPropertiesInterface =
    QStringLiteral("org.freedesktop.DBus.Properties");
const QString kPlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");

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

SpotifyClient::SpotifyClient(QObject *parent) : QObject(parent) {
  // QDBusServiceWatcher only matches exact bus names, but spotifyd's MPRIS2
  // name carries its PID and changes on restart. Watch every prefixed name we
  // discover, and re-list periodically to catch new PIDs' replacements.
  if (QDBusConnection::sessionBus().isConnected()) {
    m_watcher = new QDBusServiceWatcher(
        QString(), QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            &SpotifyClient::onServiceOwnerChanged);
  }

  m_rescanTimer.setInterval(5000);
  connect(&m_rescanTimer, &QTimer::timeout, this, &SpotifyClient::discoverServices);
  m_rescanTimer.start();

  QTimer::singleShot(0, this, &SpotifyClient::discoverServices);
}

QString SpotifyClient::title() const { return m_title; }
QString SpotifyClient::artist() const { return m_artist; }
QString SpotifyClient::album() const { return m_album; }
QString SpotifyClient::artUrl() const { return m_artUrl; }
qint64 SpotifyClient::position() const { return m_position; }
qint64 SpotifyClient::duration() const { return m_duration; }
bool SpotifyClient::isSpotifyPlaying() const { return m_isSpotifyPlaying; }
bool SpotifyClient::hasTrack() const { return m_hasTrack; }
bool SpotifyClient::isAvailable() const {
  return !m_mprisService.isEmpty() || m_daemonPresent;
}

void SpotifyClient::play() {
  if (m_mprisService.isEmpty()) {
    return;
  }
  QDBusMessage msg = QDBusMessage::createMethodCall(m_mprisService, kPlayerPath,
                                                    kPlayerInterface, "Play");
  QDBusConnection::sessionBus().send(msg);
}

void SpotifyClient::pause() {
  if (m_mprisService.isEmpty()) {
    return;
  }
  QDBusMessage msg = QDBusMessage::createMethodCall(m_mprisService, kPlayerPath,
                                                    kPlayerInterface, "Pause");
  QDBusConnection::sessionBus().send(msg);
}

void SpotifyClient::next() {
  if (m_mprisService.isEmpty()) {
    return;
  }
  QDBusMessage msg = QDBusMessage::createMethodCall(m_mprisService, kPlayerPath,
                                                    kPlayerInterface, "Next");
  QDBusConnection::sessionBus().send(msg);
}

void SpotifyClient::previous() {
  if (m_mprisService.isEmpty()) {
    return;
  }
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPlayerInterface, "Previous");
  QDBusConnection::sessionBus().send(msg);
}

void SpotifyClient::seek(qint64 positionMs) {
  if (m_mprisService.isEmpty() || m_trackId.path().isEmpty()) {
    return;
  }
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPlayerInterface, "SetPosition");
  msg << QVariant::fromValue(m_trackId) << (positionMs * 1000);
  QDBusConnection::sessionBus().send(msg);
}

void SpotifyClient::setAvailableForTest(bool available) {
  m_mprisService = available ? QStringLiteral("org.mpris.MediaPlayer2.spotifyd.test")
                             : QString();
  setAvailable(available);
}

void SpotifyClient::setHasTrackForTest(bool hasTrack) {
  setTrackPresence(hasTrack);
}

void SpotifyClient::discoverServices() {
  auto *bus = QDBusConnection::sessionBus().interface();
  QDBusReply<QStringList> reply = bus->registeredServiceNames();
  if (!reply.isValid()) {
    return;
  }

  bool daemonSeen = false;
  bool sessionSeen = false;
  for (const QString &name : reply.value()) {
    if (name.startsWith(kMprisSessionPrefix)) {
      m_mprisService = name;
      if (m_watcher) {
        m_watcher->addWatchedService(name);
      }
      // Bind the PropertiesChanged match to the *unique* owner name so Qt
      // never has to resolve a well-known name mid-connect (which is what
      // spams "Could not connect org.freedesktop.DBus.Properties to
      // onMprisPropertiesChanged"). Only (re)arm when the owner actually
      // changes; a steady session must not be re-subscribed every poll.
      QString owner = bus->serviceOwner(name).value();
      if (owner.isEmpty()) {
        owner = name;
      }
      if (owner != m_subscribedUnique) {
        m_subscribedUnique = owner;
        subscribeToMpris();
        fetchInitialMprisState();
      }
      sessionSeen = true;
      setAvailable(true);
    } else if (name.startsWith(kSpotifydDaemonPrefix)) {
      daemonSeen = true;
      if (m_watcher) {
        m_watcher->addWatchedService(name);
      }
    }
  }
  setDaemonPresent(daemonSeen);
  if (daemonSeen && !sessionSeen) {
    setAvailable(true);
  }
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

  // Session MPRIS name. newOwner is the name's *unique* owner — use it directly
  // for the signal match so no well-known-name resolution race can occur.
  if (!newOwner.isEmpty()) {
    m_mprisService = name;
    if (m_watcher) {
      m_watcher->addWatchedService(name);
    }
    if (newOwner != m_subscribedUnique) {
      m_subscribedUnique = newOwner;
      subscribeToMpris();
      fetchInitialMprisState();
    }
    setAvailable(true);
  } else {
    m_mprisService.clear();
    m_subscribedUnique.clear();
    m_trackId = QDBusObjectPath();
    setTrackPresence(false);
    unsubscribeFromMpris();
    if (!m_daemonPresent) {
      setAvailable(false);
    }
  }
}

void SpotifyClient::subscribeToMpris() {
  QDBusConnection::sessionBus().connect(
      m_subscribedUnique, kPlayerPath, kPropertiesInterface, kPropertiesChanged,
      this, SLOT(onMprisPropertiesChanged(QString, QVariantMap, QStringList)));
}

void SpotifyClient::unsubscribeFromMpris() {
  QDBusConnection::sessionBus().disconnect(
      m_subscribedUnique, kPlayerPath, kPropertiesInterface, kPropertiesChanged,
      this, SLOT(onMprisPropertiesChanged(QString, QVariantMap, QStringList)));
}

void SpotifyClient::fetchInitialMprisState() {
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPropertiesInterface, "GetAll");
  msg << kPlayerInterface;
  QDBusReply<QVariantMap> reply = QDBusConnection::sessionBus().call(msg);

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

void SpotifyClient::onMprisPropertiesChanged(
    const QString &interface, const QVariantMap &changed,
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
    }
  }
  if (metadata.contains(kArtistKey)) {
    QString v;
    if (metadata[kArtistKey].canConvert<QStringList>()) {
      v = metadata[kArtistKey].toStringList().join(", ");
    } else {
      v = metadata[kArtistKey].toString();
    }
    if (m_artist != v) {
      m_artist = v;
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
