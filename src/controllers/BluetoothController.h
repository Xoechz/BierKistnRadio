#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <qqmlintegration.h>

class BluetoothController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(BluetoothController)

  Q_PROPERTY(bool discoverable READ discoverable NOTIFY discoverableChanged)
  Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName NOTIFY
                 pairedDeviceNameChanged)
  Q_PROPERTY(QStringList devices READ devices NOTIFY devicesChanged)

public:
  explicit BluetoothController(QObject *parent = nullptr);

  bool discoverable() const;
  QString pairedDeviceName() const;
  QStringList devices() const;

  Q_INVOKABLE void setDiscoverable(bool on);
  Q_INVOKABLE void pair(const QString &address);
  Q_INVOKABLE void connectDevice(const QString &address);
  Q_INVOKABLE void disconnectDevice(const QString &address);

signals:
  void discoverableChanged();
  void pairedDeviceNameChanged();
  void devicesChanged();

private:
  bool m_discoverable = false;
  QString m_pairedDeviceName;
  QStringList m_devices;
};
