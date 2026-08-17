#pragma once

#include <QObject>
#include <qqmlintegration.h>

class PowerController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(PowerController)

public:
  explicit PowerController(QObject *parent = nullptr);

  Q_INVOKABLE void reboot();
  Q_INVOKABLE void shutdown();
};