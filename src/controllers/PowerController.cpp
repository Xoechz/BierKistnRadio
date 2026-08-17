#include "PowerController.h"

#include <QProcess>

PowerController::PowerController(QObject *parent) : QObject(parent) {}

void PowerController::reboot() {
  QProcess::startDetached(QStringLiteral("systemctl"),
                          {QStringLiteral("reboot")});
}

void PowerController::shutdown() {
  QProcess::startDetached(QStringLiteral("systemctl"),
                          {QStringLiteral("poweroff")});
}