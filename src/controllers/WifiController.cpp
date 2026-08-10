#include "WifiController.h"

WifiController::WifiController(QObject *parent) : QObject(parent) {}

bool WifiController::connected() const { return m_connected; }
QString WifiController::ssid() const { return m_ssid; }
int WifiController::signalStrength() const { return m_signalStrength; }
QStringList WifiController::networks() const { return m_networks; }

void WifiController::scan() {}
void WifiController::connect(const QString &, const QString &) {}
void WifiController::disconnect() {}
