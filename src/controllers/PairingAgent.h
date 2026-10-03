#pragma once

#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QTimer>
#include <functional>
#include <qqmlintegration.h>

// Owned by BluetoothClient. Only Agent1's scriptable slots are exported on
// D-Bus; touchscreen decisions are deliberately not remotely callable.
class PairingAgent : public QObject, protected QDBusContext {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")
  QML_NAMED_ELEMENT(PairingAgent)
  QML_UNCREATABLE("PairingAgent is owned by BluetoothClient")
  Q_PROPERTY(bool pending READ pending NOTIFY promptChanged)
  Q_PROPERTY(QString deviceName READ deviceName NOTIFY promptChanged)
  Q_PROPERTY(QString passkey READ passkey NOTIFY promptChanged)
  Q_PROPERTY(int secondsRemaining READ secondsRemaining NOTIFY promptChanged)
  Q_PROPERTY(quint64 requestId READ requestId NOTIFY promptChanged)

public:
  explicit PairingAgent(const QDBusConnection &bus, QObject *parent = nullptr);
  ~PairingAgent() override;
  void setEnabled(bool enabled);
  void setDeviceLookup(std::function<QString(const QString &)> name,
                       std::function<bool(const QString &)> paired);
  bool pending() const { return !m_requests.isEmpty(); }
  QString deviceName() const { return m_name; }
  QString passkey() const { return m_code; }
  int secondsRemaining() const { return m_seconds; }
  quint64 requestId() const { return m_requestId; }
  Q_INVOKABLE void resolve(quint64 requestId, bool accept);

public slots:
  Q_SCRIPTABLE void Release();
  Q_SCRIPTABLE void Cancel();
  Q_SCRIPTABLE void RequestConfirmation(const QDBusObjectPath &device, uint passkey);
  Q_SCRIPTABLE void DisplayPasskey(const QDBusObjectPath &device, uint passkey,
                                  ushort entered);
  Q_SCRIPTABLE QString RequestPinCode(const QDBusObjectPath &device);
  Q_SCRIPTABLE uint RequestPasskey(const QDBusObjectPath &device);
  Q_SCRIPTABLE void DisplayPinCode(const QDBusObjectPath &device, const QString &pin);
  Q_SCRIPTABLE void RequestAuthorization(const QDBusObjectPath &device);
  Q_SCRIPTABLE void AuthorizeService(const QDBusObjectPath &device, const QString &uuid);

signals:
  void promptChanged();
  void errorChanged(const QString &error);

private:
  bool trustedCall();
  void request(const QDBusObjectPath &device, uint passkey);
  void finish(bool accept, const QString &reason);
  void cancelPairing(const QString &device);
  void registerAgent();
  void unregisterAgent();
  void unsupported();
  QDBusConnection m_bus;
  QString m_owner;
  bool m_enabled = false;
  bool m_exported = false;
  quint64 m_generation = 0;
  quint64 m_requestId = 0;
  QList<QDBusMessage> m_requests;
  QString m_device;
  QString m_name;
  QString m_code;
  int m_seconds = 0;
  QTimer m_deadline;
  QTimer m_countdown;
  std::function<QString(const QString &)> m_nameLookup;
  std::function<bool(const QString &)> m_pairedLookup;
};
