#include "SpotifyClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QTimer>

namespace {
const QString kMprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.spotifyd.");

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
  connect(QDBusConnection::sessionBus().interface(),
          &QDBusConnectionInterface::serviceOwnerChanged, this,
          &SpotifyClient::onServiceOwnerChanged);

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
bool SpotifyClient::isAvailable() const { return !m_mprisService.isEmpty(); }

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

  for (const QString &name : reply.value()) {
    if (name.startsWith(kMprisPrefix)) {
      m_mprisService = name;
      subscribeToMpris();
      fetchInitialMprisState();
      setAvailable(true);
    }
  }
}

void SpotifyClient::onServiceOwnerChanged(const QString &name,
                                          const QString &oldOwner,
                                          const QString &newOwner) {
  Q_UNUSED(oldOwner);
  if (!name.startsWith(kMprisPrefix)) {
    return;
  }

  if (!newOwner.isEmpty()) {
    m_mprisService = name;
    subscribeToMpris();
    fetchInitialMprisState();
    setAvailable(true);
  } else {
    m_mprisService.clear();
    m_trackId = QDBusObjectPath();
    setTrackPresence(false);
    unsubscribeFromMpris();
    setAvailable(false);
  }
}

void SpotifyClient::subscribeToMpris() {
  QDBusConnection::sessionBus().connect(
      m_mprisService, kPlayerPath, kPropertiesInterface, kPropertiesChanged,
      this, SLOT(onMprisPropertiesChanged(QString, QVariantMap, QStringList)));
}

void SpotifyClient::unsubscribeFromMpris() {
  QDBusConnection::sessionBus().disconnect(
      m_mprisService, kPlayerPath, kPropertiesInterface, kPropertiesChanged,
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
