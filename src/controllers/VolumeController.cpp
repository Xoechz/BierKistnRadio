#include "VolumeController.h"

#include <QProcess>

VolumeController::VolumeController(QObject *parent) : QObject(parent) {}

int VolumeController::volume() const { return m_volume; }

void VolumeController::setVolume(int percent) {
  percent = std::clamp(percent, 0, 150);

  if (m_volume == percent) {
    return;
  }

  m_volume = percent;
  emit volumeChanged();

  QProcess::execute("wpctl", {"set-volume", "@DEFAULT_AUDIO_SINK@",
                              QString::number(percent) + "%"});
}

void VolumeController::increaseVolume() { setVolume(m_volume + 5); }

void VolumeController::decreaseVolume() { setVolume(m_volume - 5); }
