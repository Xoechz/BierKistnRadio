#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

class BluetoothController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(BluetoothController)

  Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY
                 connectedDeviceNameChanged)
  Q_PROPERTY(bool takeoverPending READ takeoverPending NOTIFY
                 takeoverPendingChanged)

public:
  enum TakeoverChoice { KeepCurrent, SwitchToNew };
  Q_ENUM(TakeoverChoice)

  explicit BluetoothController(QObject *parent = nullptr);

  QString connectedDeviceName() const;
  bool takeoverPending() const;

  Q_INVOKABLE void resolveTakeover(TakeoverChoice choice);

signals:
  void connectedDeviceNameChanged();
  void takeoverPendingChanged();

private:
  QString m_connectedDeviceName;
  bool m_takeoverPending = false;
};
