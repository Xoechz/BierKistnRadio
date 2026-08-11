#include "BluetoothController.h"

BluetoothController::BluetoothController(QObject *parent) : QObject(parent) {}

QString BluetoothController::connectedDeviceName() const {
  return m_connectedDeviceName;
}

bool BluetoothController::takeoverPending() const { return m_takeoverPending; }

void BluetoothController::resolveTakeover(TakeoverChoice) {}
