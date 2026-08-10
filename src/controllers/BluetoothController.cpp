#include "BluetoothController.h"

BluetoothController::BluetoothController(QObject *parent) : QObject(parent) {}

bool BluetoothController::discoverable() const { return m_discoverable; }
QString BluetoothController::pairedDeviceName() const {
  return m_pairedDeviceName;
}
QStringList BluetoothController::devices() const { return m_devices; }

void BluetoothController::setDiscoverable(bool) {}
void BluetoothController::pair(const QString &) {}
void BluetoothController::connectDevice(const QString &) {}
void BluetoothController::disconnectDevice(const QString &) {}
