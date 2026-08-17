#include "PlaybackController.h"

#include "BluetoothClient.h"
#include "SpotifyClient.h"

PlaybackController::PlaybackController(QObject *parent) : QObject(parent) {
  m_spotify = new SpotifyClient(this);
  m_bluetooth = new BluetoothClient(this);

  connect(m_spotify, &SpotifyClient::availableChanged, this,
          &PlaybackController::onSpotifyChanged);
  connect(m_spotify, &SpotifyClient::hasTrackChanged, this,
          &PlaybackController::onSpotifyChanged);
  connect(m_spotify, &SpotifyClient::isSpotifyPlayingChanged, this,
          &PlaybackController::onSpotifyChanged);

  connect(m_bluetooth, &BluetoothClient::connectedDeviceNameChanged, this,
          &PlaybackController::onBluetoothChanged);

  refreshSpotifyState();
}

PlaybackController::PlaybackState PlaybackController::playbackState() const {
  return m_playbackState;
}

bool PlaybackController::isBluetoothActive() const {
  return m_playbackState == BluetoothActive;
}

SpotifyClient *PlaybackController::spotify() const { return m_spotify; }

BluetoothClient *PlaybackController::bluetooth() const { return m_bluetooth; }

void PlaybackController::play() {
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->play();
  } else {
    m_spotify->play();
  }
}

void PlaybackController::pause() {
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->pause();
  } else {
    m_spotify->pause();
  }
}

void PlaybackController::next() {
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->next();
  } else {
    m_spotify->next();
  }
}

void PlaybackController::previous() {
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->previous();
  } else {
    m_spotify->previous();
  }
}

void PlaybackController::seek(qint64 positionMs) {
  // AVRCP has no seek-absolute; seek is Spotify only (ADR 0006).
  m_spotify->seek(positionMs);
}

void PlaybackController::switchToSpotify() {
  // Hard-mute + AVRCP-pause the BT side, then recompute Spotify state
  // (ADR 0008: mute-before-pause, no frame relies on the phone).
  if (m_playbackState == BluetoothActive ||
      m_playbackState == BluetoothWaiting) {
    m_bluetooth->setMuted(true);
    m_bluetooth->pauseAll();
  }

  refreshSpotifyState();
}

void PlaybackController::switchToBluetooth() {
  // Pause spotifyd (its output shares the physical sink; pause IS its mute).
  m_spotify->pause();
  // Unmute only the active BT node.
  m_bluetooth->setMuted(false);

  if (m_playbackState == BluetoothActive ||
      !m_bluetooth->connectedDeviceName().isEmpty()) {
    setPlaybackState(BluetoothActive);
  } else {
    setPlaybackState(BluetoothWaiting);

    m_bluetooth->ensureDiscoverable();
  }
}

void PlaybackController::onSpotifyChanged() {
  // While Bluetooth is the audible Source, a spotify session that becomes
  // playable must be silenced (ADR 0008: inactive Source muted+paused). The
  // visible Source stays Bluetooth; the stream is paused behind a "· Muted"
  // chip on the OTHER side.
  if (m_playbackState == BluetoothWaiting ||
      m_playbackState == BluetoothActive) {
    if (m_spotify->isSpotifyPlaying()) {
      m_spotify->pause();
    }
    return;
  }

  refreshSpotifyState();
}

void PlaybackController::onBluetoothChanged() {
  bool connected = !m_bluetooth->connectedDeviceName().isEmpty();

  if (connected) {
    onBluetoothConnected();
  } else {
    onBluetoothDisconnected();
  }
}

void PlaybackController::onBluetoothConnected() {
  // A BT stream appearing while in a Spotify state: mute + AVRCP-pause it
  // (ADR 0008 invariant), but do NOT change the source.
  if (m_playbackState != BluetoothWaiting &&
      m_playbackState != BluetoothActive) {
    m_bluetooth->setMuted(true);
    m_bluetooth->pauseAll();
  } else if (m_playbackState == BluetoothWaiting) {
    m_bluetooth->setMuted(false); // prevent muted BT stream
    setPlaybackState(BluetoothActive);
  } else if (m_playbackState == BluetoothActive) {
    // connection takeover
  }
}

void PlaybackController::onBluetoothDisconnected() {
  // BT disconnect while in BluetoothActive: re-query the BT subtree. If
  // another device is still connected, the client retargets at it and we
  // stay BluetoothActive; otherwise drop to BluetoothWaiting. Never touch
  // the Spotify state on a BT disconnect — source switching is explicit.
  if (m_playbackState == BluetoothActive) {
    bool stillConnected = !m_bluetooth->connectedDeviceName().isEmpty();
    setPlaybackState(stillConnected ? BluetoothActive : BluetoothWaiting);
  }
}

void PlaybackController::refreshSpotifyState() {
  PlaybackState next;
  if (!m_spotify->isAvailable()) {
    next = SpotifyUnavailable;
  } else if (m_spotify->hasTrack()) {
    next = SpotifyActive;
  } else {
    next = SpotifyWaiting;
  }
  setPlaybackState(next);
}

void PlaybackController::setPlaybackState(PlaybackState next) {
  if (m_playbackState == next) {
    return;
  }
  bool wasBt = (m_playbackState == BluetoothActive);
  m_playbackState = next;
  emit playbackStateChanged();
  if (wasBt != (next == BluetoothActive)) {
    emit isBluetoothActiveChanged();
  }
}
