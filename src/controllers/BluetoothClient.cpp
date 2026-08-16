#include "BluetoothClient.h"

BluetoothClient::BluetoothClient(QObject *parent) : QObject(parent) {}

QString BluetoothClient::connectedDeviceName() const {
  return m_connectedDeviceName;
}

bool BluetoothClient::takeoverPending() const { return m_takeoverPending; }

bool BluetoothClient::statusPublished() const { return m_statusPublished; }
bool BluetoothClient::trackPublished() const { return m_trackPublished; }
QString BluetoothClient::trackTitle() const { return m_trackTitle; }
QString BluetoothClient::trackArtist() const { return m_trackArtist; }
QString BluetoothClient::trackAlbum() const { return m_trackAlbum; }
qint64 BluetoothClient::position() const { return m_position; }
qint64 BluetoothClient::duration() const { return m_duration; }
bool BluetoothClient::positionPublished() const { return m_positionPublished; }
bool BluetoothClient::isBluetoothPlaying() const { return m_isBluetoothPlaying; }
bool BluetoothClient::muted() const { return m_muted; }

void BluetoothClient::resolveTakeover(TakeoverChoice) {}

void BluetoothClient::ensureDiscoverable() {}

void BluetoothClient::play() {}
void BluetoothClient::pause() {}
void BluetoothClient::next() {}
void BluetoothClient::previous() {}

void BluetoothClient::setMuted(bool muted) {
  if (m_muted == muted) {
    return;
  }
  m_muted = muted;
  emit mutedChanged();
}
