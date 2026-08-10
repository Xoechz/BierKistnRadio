#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <qqmlintegration.h>

class WifiController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(WifiController)

  Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
  Q_PROPERTY(QString ssid READ ssid NOTIFY ssidChanged)
  Q_PROPERTY(
      int signalStrength READ signalStrength NOTIFY signalStrengthChanged)
  Q_PROPERTY(QStringList networks READ networks NOTIFY networksChanged)

public:
  explicit WifiController(QObject *parent = nullptr);

  bool connected() const;
  QString ssid() const;
  int signalStrength() const;
  QStringList networks() const;

  Q_INVOKABLE void scan();
  Q_INVOKABLE void connect(const QString &ssid, const QString &password);
  Q_INVOKABLE void disconnect();

signals:
  void connectedChanged();
  void ssidChanged();
  void signalStrengthChanged();
  void networksChanged();

private:
  bool m_connected = false;
  QString m_ssid;
  int m_signalStrength = 0;
  QStringList m_networks;
};
