#pragma once

#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <qqmlintegration.h>

class WifiController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(WifiController)

  Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
  Q_PROPERTY(QString ssid READ ssid NOTIFY ssidChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
  Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)
  Q_PROPERTY(
      int signalStrength READ signalStrength NOTIFY signalStrengthChanged)
  Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)

public:
  explicit WifiController(QObject *parent = nullptr);

  bool connected() const;
  QString ssid() const;
  QString errorMessage() const;
  bool connecting() const;
  int signalStrength() const;
  QVariantList networks() const;

  Q_INVOKABLE void scan();
  Q_INVOKABLE void connect(const QString &ssid, const QString &password);
  Q_INVOKABLE void disconnect();

  // NetworkManager D-Bus call seam. `onFinished(reply, error)` receives the
  // unwrapped reply value (or empty on error) plus a non-empty error string on
  // D-Bus / method failure. Injectable so tests never need a live daemon.
  using DbusCallable = std::function<void(
      const QString &service, const QString &objectPath,
      const QString &interface, const QString &method, const QVariantList &args,
      const std::function<void(const QVariant &reply, const QString &error)> &onFinished)>;
  void setDbusCallableForTest(const DbusCallable &callable);

  // Pure helpers (unit-testable without a D-Bus peer).
  using SettingsMap = QMap<QString, QVariantMap>; // D-Bus a{sa{sv}}
  static QString ssidFromVariant(const QVariant &ssidVariant);
  static bool accessPointSecured(const QVariantMap &props);
  static int accessPointStrength(const QVariantMap &props);
  static QVariantMap connectionSettings(const QString &ssid,
                                        const QString &password, bool secured);

  // Test hooks: deliver NetworkManager AccessPoint signals the way the real
  // D-Bus subscription does at runtime.
  void accessPointAddedForTest(const QString &apPath, const QVariantMap &props);
  void accessPointRemovedForTest(const QString &apPath);

  // Refreshes the active-connection state by re-querying the bus; used by the
  // single-shot startup refresh and by tests to drive a re-query.
  Q_INVOKABLE void refreshActiveConnection();

signals:
  void connectedChanged();
  void ssidChanged();
  void errorMessageChanged();
  void connectingChanged();
  void signalStrengthChanged();
  void networksChanged();
  void connectionSucceeded(const QString &ssid);

private:
  void setError(const QString &message);
  void setConnecting(bool connecting);
  void rebuildNetworks();
  void discoverWifiDevice();
  void findWifiDevice(const QList<QDBusObjectPath> &devices, int index,
                      quint64 generation);
  void subscribeAccessPoints(const QString &devicePath);
  void requestScan(const QString &devicePath);
  void refreshAccessPoints(const QString &devicePath, quint64 generation);
  void fetchAccessPoint(const QString &apPath, quint64 generation);
  void fetchAccessPointState(const QString &apPath);
  void setWifiDevicePath(const QString &path);
  void failConnection(const QString &message);
  static QString dbusErrorText(const QString &operation, const QString &error);
  void setConnectedState(bool connected, const QString &ssid, int signalStrength);

  DbusCallable m_dbusCall;
  QVariantMap m_accessPoints; // apPath -> props (Ssid/Strength/Flags/WpaFlags/RsnFlags)
  QVariantList m_networks;

  QString m_wifiDevicePath;
  QString m_connectRequestedSsid;
  QSet<QString> m_knownAccessPoints;
  quint64 m_scanGeneration = 0;
  quint64 m_inventoryGeneration = 0;
  quint64 m_stateGeneration = 0;
  quint64 m_connectGeneration = 0;
  QTimer m_connectTimer;
  bool m_connected = false;
  bool m_connecting = false;
  QString m_ssid;
  QString m_errorMessage;
  int m_signalStrength = 0;

private slots:
  void onAccessPointAdded(const QDBusObjectPath &path);
  void onAccessPointRemoved(const QDBusObjectPath &path);
  void onPropertiesChanged(const QString &interface,
                            const QVariantMap &changedProperties,
                            const QStringList &invalidatedProperties);
  void onWifiPropertiesChanged(const QString &interface,
                               const QVariantMap &changedProperties,
                               const QStringList &invalidatedProperties);
  void onWifiDeviceStateChanged(uint newState, uint oldState, uint reason);
};
