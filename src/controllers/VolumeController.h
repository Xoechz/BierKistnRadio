#pragma once

#include <QObject>
#include <qqmlintegration.h>

class VolumeController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(VolumeController)

  Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)

public:
  explicit VolumeController(QObject *parent = nullptr);

  int volume() const;

  Q_INVOKABLE void setVolume(int percent);
  Q_INVOKABLE void increaseVolume();
  Q_INVOKABLE void decreaseVolume();

signals:
  void volumeChanged();

private:
  int m_volume = 50;
};
