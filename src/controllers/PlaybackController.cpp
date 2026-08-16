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
  // Mute the BT stream first (hard silence), then best-effort pause, then
  // recompute the Spotify state from the bus (ADR 0006 mute-before-pause).
  if (m_playbackState == BluetoothActive) {
    m_bluetooth->setMuted(true);
    m_bluetooth->pause();
  }

  refreshSpotifyState();
}

void PlaybackController::switchToBluetooth() {
  m_spotify->pause();
  m_bluetooth->setMuted(false);

  if (m_playbackState == BluetoothActive ||
      !m_bluetooth->connectedDeviceName().isEmpty()) {
    setPlaybackState(BluetoothActive);
  } else {
    setPlaybackState(BluetoothWaiting);
  }
}

void PlaybackController::onSpotifyChanged() {
  if (m_playbackState == BluetoothWaiting ||
      m_playbackState == BluetoothActive) {
    return;
  }

  refreshSpotifyState();
}

void PlaybackController::onBluetoothChanged() {
  bool connected = !m_bluetooth->connectedDeviceName().isEmpty();

  if (connected) {
    // A BT stream appearing while in a Spotify state: mute it (ADR 0006
    // invariant), but do NOT change the source. Connection state is unchanged.
    if (m_playbackState != BluetoothWaiting &&
        m_playbackState != BluetoothActive) {
      m_bluetooth->setMuted(true);
    } else if (m_playbackState == BluetoothWaiting) {
      setPlaybackState(BluetoothActive);
    }
  } else {
    // BT disconnect while in BluetoothActive: re-query -> a Spotify state.
    if (m_playbackState == BluetoothActive) {
      refreshSpotifyState();
    }
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
