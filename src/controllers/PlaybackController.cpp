#include "PlaybackController.h"

PlaybackController::PlaybackController(QObject *parent) : QObject(parent) {}

QString PlaybackController::title() const { return m_title; }
QString PlaybackController::artist() const { return m_artist; }
QString PlaybackController::album() const { return m_album; }
QString PlaybackController::artUrl() const { return m_artUrl; }
qint64 PlaybackController::position() const { return m_position; }
qint64 PlaybackController::duration() const { return m_duration; }
bool PlaybackController::isPlaying() const { return m_isPlaying; }
bool PlaybackController::isStation() const { return m_isStation; }
bool PlaybackController::isSinkMode() const { return m_isSinkMode; }
QString PlaybackController::pairedDeviceName() const {
  return m_pairedDeviceName;
}

void PlaybackController::play() {}
void PlaybackController::pause() {}
void PlaybackController::next() {}
void PlaybackController::previous() {}
void PlaybackController::seek(qint64) {}
