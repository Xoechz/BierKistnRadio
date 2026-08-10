#include "PlaybackController.h"

PlaybackController::PlaybackController(QObject *parent) : QObject(parent) {}

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

void PlaybackController::play() {}
void PlaybackController::pause() {}
void PlaybackController::next() {}
void PlaybackController::previous() {}
void PlaybackController::seek(qint64) {}
void PlaybackController::transferPlayback() {}

void PlaybackController::switchToBluetooth() {
  pause();
}

void PlaybackController::switchToSpotify() {}
