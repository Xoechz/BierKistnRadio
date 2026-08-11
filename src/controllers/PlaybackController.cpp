#include "PlaybackController.h"

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
const QString kNoTrackTrackId = QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");

const QString kPlaying = QStringLiteral("Playing");
} // namespace

PlaybackController::PlaybackController(QObject *parent) : QObject(parent) {
  connect(QDBusConnection::sessionBus().interface(),
          &QDBusConnectionInterface::serviceOwnerChanged, this,
          &PlaybackController::onServiceOwnerChanged);

  QTimer::singleShot(0, this, &PlaybackController::discoverServices);
}

PlaybackController::PlaybackState PlaybackController::playbackState() const {
  return m_playbackState;
}

QString PlaybackController::title() const { return m_title; }
QString PlaybackController::artist() const { return m_artist; }
QString PlaybackController::album() const { return m_album; }
QString PlaybackController::artUrl() const { return m_artUrl; }
qint64 PlaybackController::position() const { return m_position; }
qint64 PlaybackController::duration() const { return m_duration; }
bool PlaybackController::isSpotifyPlaying() const { return m_isSpotifyPlaying; }
bool PlaybackController::isBluetoothActive() const {
  return m_playbackState == BluetoothActive;
}
QString PlaybackController::pairedDeviceName() const {
  return m_pairedDeviceName;
}

void PlaybackController::play() {
  if (m_mprisService.isEmpty())
    return;
  QDBusMessage msg = QDBusMessage::createMethodCall(m_mprisService, kPlayerPath,
                                                    kPlayerInterface, "Play");
  QDBusConnection::sessionBus().send(msg);
}

void PlaybackController::pause() {
  if (m_mprisService.isEmpty())
    return;
  QDBusMessage msg = QDBusMessage::createMethodCall(m_mprisService, kPlayerPath,
                                                    kPlayerInterface, "Pause");
  QDBusConnection::sessionBus().send(msg);
}

void PlaybackController::next() {
  if (m_mprisService.isEmpty())
    return;
  QDBusMessage msg = QDBusMessage::createMethodCall(m_mprisService, kPlayerPath,
                                                    kPlayerInterface, "Next");
  QDBusConnection::sessionBus().send(msg);
}

void PlaybackController::previous() {
  if (m_mprisService.isEmpty())
    return;
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPlayerInterface, "Previous");
  QDBusConnection::sessionBus().send(msg);
}

void PlaybackController::seek(qint64 positionMs) {
  if (m_mprisService.isEmpty() || m_trackId.path().isEmpty())
    return;
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPlayerInterface, "SetPosition");
  msg << QVariant::fromValue(m_trackId) << (positionMs * 1000);
  QDBusConnection::sessionBus().send(msg);
}

void PlaybackController::switchToBluetooth() { pause(); }

void PlaybackController::switchToSpotify() {
  refreshSpotifyState(true);
  m_pairedDeviceName.clear();
  emit pairedDeviceNameChanged();
}

void PlaybackController::discoverServices() {
  auto *bus = QDBusConnection::sessionBus().interface();
  QDBusReply<QStringList> reply = bus->registeredServiceNames();
  if (!reply.isValid())
    return;

  for (const QString &name : reply.value()) {
    if (name.startsWith(kMprisPrefix)) {
      m_mprisService = name;
      subscribeToMpris();
      fetchInitialMprisState();
      refreshSpotifyState();
    }
  }
}

void PlaybackController::onServiceOwnerChanged(const QString &name,
                                               const QString &oldOwner,
                                               const QString &newOwner) {
  if (!name.startsWith(kMprisPrefix))
    return;

  if (!newOwner.isEmpty()) {
    m_mprisService = name;
    subscribeToMpris();
    fetchInitialMprisState();
    refreshSpotifyState();
  } else {
    m_mprisService.clear();
    m_trackId = QDBusObjectPath();
    m_hasTrack = false;
    unsubscribeFromMpris();
    refreshSpotifyState();
  }
}

void PlaybackController::subscribeToMpris() {
  QDBusConnection::sessionBus().connect(
      m_mprisService, kPlayerPath,
      kPropertiesInterface, kPropertiesChanged,
      this, SLOT(onMprisPropertiesChanged(QString, QVariantMap, QStringList)));
}

void PlaybackController::unsubscribeFromMpris() {
  QDBusConnection::sessionBus().disconnect(
      m_mprisService, kPlayerPath,
      kPropertiesInterface, kPropertiesChanged,
      this, SLOT(onMprisPropertiesChanged(QString, QVariantMap, QStringList)));
}

void PlaybackController::fetchInitialMprisState() {
  QDBusMessage msg = QDBusMessage::createMethodCall(
      m_mprisService, kPlayerPath, kPropertiesInterface, "GetAll");
  msg << kPlayerInterface;
  QDBusReply<QVariantMap> reply = QDBusConnection::sessionBus().call(msg);

  if (!reply.isValid()) {
    return;
  }

  QVariantMap props = reply.value();
  if (props.contains(kPlaybackStatus))
    updatePlaybackStatus(props[kPlaybackStatus].toString());
  if (props.contains(kMetadata)) {
    QDBusArgument arg = props[kMetadata].value<QDBusArgument>();
    updateFromMetadata(qdbus_cast<QVariantMap>(arg));
  }
  if (props.contains(kPosition)) {
    m_position = props[kPosition].toLongLong() / 1000;
    emit positionChanged();
  }
}

void PlaybackController::onMprisPropertiesChanged(
    const QString &interface, const QVariantMap &changed,
    const QStringList &invalidated) {
  Q_UNUSED(invalidated);

  if (interface != kPlayerInterface)
    return;

  if (changed.contains(kPlaybackStatus))
    updatePlaybackStatus(changed[kPlaybackStatus].toString());

  if (changed.contains(kMetadata)) {
    QDBusArgument arg =
        changed[kMetadata].value<QDBusArgument>();
    updateFromMetadata(qdbus_cast<QVariantMap>(arg));
  }

  if (changed.contains(kPosition)) {
    m_position = changed[kPosition].toLongLong() / 1000;
    emit positionChanged();
  }

  refreshSpotifyState();
}

void PlaybackController::updateFromMetadata(const QVariantMap &metadata) {
  if (metadata.contains(kTitleKey)) {
    QString v = metadata[kTitleKey].toString();
    if (m_title != v) {
      m_title = v;
      emit titleChanged();
    }
  }
  if (metadata.contains(kArtistKey)) {
    QString v;
    if (metadata[kArtistKey].canConvert<QStringList>())
      v = metadata[kArtistKey].toStringList().join(", ");
    else
      v = metadata[kArtistKey].toString();
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
    m_hasTrack = !m_trackId.path().isEmpty() && m_trackId.path() != kNoTrackTrackId;
  }
}

void PlaybackController::updatePlaybackStatus(const QString &status) {
  bool playing = (status == kPlaying);
  if (m_isSpotifyPlaying != playing) {
    m_isSpotifyPlaying = playing;
    emit isSpotifyPlayingChanged();
  }
  if (status == QStringLiteral("Stopped"))
    m_hasTrack = false;
}

void PlaybackController::setPlaybackState(PlaybackState next) {
  if (m_playbackState == next)
    return;
  bool wasBt = (m_playbackState == BluetoothActive);
  m_playbackState = next;
  emit playbackStateChanged();
  if (wasBt != (next == BluetoothActive))
    emit isBluetoothActiveChanged();
}

void PlaybackController::refreshSpotifyState(bool force) {
  if (!force) {
    if (m_playbackState == BluetoothWaiting || m_playbackState == BluetoothActive)
      return;
  }

  PlaybackState next;
  if (m_mprisService.isEmpty()) {
    next = SpotifyUnavailable;
  } else if (m_hasTrack) {
    next = SpotifyActive;
  } else {
    next = SpotifyWaiting;
  }
  setPlaybackState(next);
}