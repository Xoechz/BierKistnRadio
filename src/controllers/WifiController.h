#pragma once

#include <QDBusConnection>
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
  /// Construct on the real system bus. @param parent Optional owner; no return.
  explicit WifiController(QObject *parent = nullptr);
  /// Install bounded calls, activation deadline, subscriptions and daemon recovery.
  /// @param bus NetworkManager bus. @param parent Optional owner. No return value.
  explicit WifiController(const QDBusConnection &bus, QObject *parent = nullptr);

  /// @return Cached activated Wi-Fi state; this is not internet reachability.
  bool connected() const;
  /// @return Connected SSID, or empty while disconnected.
  QString ssid() const;
  /// @return Primary operation error, then subscription error, otherwise empty.
  QString errorMessage() const;
  /// @return Whether a selected-network activation remains pending.
  bool connecting() const;
  /// @return Cached connected AP signal percentage (zero when disconnected).
  int signalStrength() const;
  /// @return Cached rows containing ssid, signalStrength and secured fields.
  QVariantList networks() const;

  /// Reset scan generation/inventory, load known APs and request fresh scan.
  /// No parameters/return; list and errors update asynchronously.
  Q_INVOKABLE void scan();
  /// Request typed NM activation and wait for observed matching SSID/state.
  /// @param ssid Selected name. @param password WPA-PSK password for secured network.
  /// No return; a successful method reply alone cannot emit connectionSucceeded.
  Q_INVOKABLE void connect(const QString &ssid, const QString &password);
  /// Request known device disconnect; no-op without path. No params/return; errors async.
  Q_INVOKABLE void disconnect();

  // NetworkManager D-Bus call seam. `onFinished(reply, error)` receives the
  // unwrapped reply value (or empty on error) plus a non-empty error string on
  // D-Bus / method failure. Injectable so tests never need a live daemon.
  using DbusCallable = std::function<void(
      const QString &service, const QString &objectPath,
      const QString &interface, const QString &method, const QVariantList &args,
      const std::function<void(const QVariant &reply, const QString &error)> &onFinished)>;
  /// @param callable Replacement typed dispatch/completion seam. No return value.
  void setDbusCallableForTest(const DbusCallable &callable);

  // Pure helpers (unit-testable without a D-Bus peer).
  using SettingsMap = QMap<QString, QVariantMap>; // D-Bus a{sa{sv}}
  /// @param ssidVariant Byte-array/argument or string-like SSID payload.
  /// @return UTF-8-decoded SSID or QVariant string conversion.
  static QString ssidFromVariant(const QVariant &ssidVariant);
  /// @param props AP flags. @return Whether privacy/WPA/RSN flags indicate security.
  static bool accessPointSecured(const QVariantMap &props);
  /// @param props AP dictionary. @return Strength converted to int; no clamping here.
  static int accessPointStrength(const QVariantMap &props);
  /// Build a nested NM infrastructure profile with automatic IPv4/IPv6.
  /// @param ssid UTF-8-encoded network name. @param password PSK if secured.
  /// @param secured Include WPA-PSK group when true. @return QVariant settings map.
  static QVariantMap connectionSettings(const QString &ssid,
                                        const QString &password, bool secured);

  // Test hooks: deliver NetworkManager AccessPoint signals the way the real
  // D-Bus subscription does at runtime.
  /// Insert/update AP cache and rebuild model; runtime fetch also uses this hook.
  /// @param apPath Object path. @param props AP dictionary. No return value.
  void accessPointAddedForTest(const QString &apPath, const QVariantMap &props);
  /// @param apPath Cache entry to remove and rebuild. No return value.
  void accessPointRemovedForTest(const QString &apPath);

  // Refreshes the active-connection state by re-querying the bus; used by the
  // single-shot startup refresh and by tests to drive a re-query.
  /// Request current device state and resolve active AP with generation guards.
  /// No parameters/return; connection properties update after replies.
  Q_INVOKABLE void refreshActiveConnection();

signals:
  /// Notify connected-state change. No parameters/return.
  void connectedChanged();
  /// Notify connected SSID change. No parameters/return.
  void ssidChanged();
  /// Notify combined visible error change. No parameters/return.
  void errorMessageChanged();
  /// Notify pending-activation change. No parameters/return.
  void connectingChanged();
  /// Notify connected AP signal change. No parameters/return.
  void signalStrengthChanged();
  /// Notify deduplicated network-model change. No parameters/return.
  void networksChanged();
  /// Publish observed selected-network activation, not just method acknowledgement.
  /// @param ssid Confirmed requested network name. No return value.
  void connectionSucceeded(const QString &ssid);

private:
  /// @param message Operation text or empty; preserves independent subscription faults.
  /// No return value; notifies only if combined visible message changes.
  void setError(const QString &message);
  /// @param key Independent concern. @param message Text or empty to remove. No return.
  void setBackgroundError(const QString &key, const QString &message);
  /// Replace/check root NM PropertiesChanged match. No parameters/return.
  void subscribeManager();
  /// Replace one exact signal match and record subscription failure. No return.
  /// @param key Error concern. @param path Object path. @param interface D-Bus interface.
  /// @param signal Signal name. @param slot Qt SLOT signature of its receiver.
  void subscribeSignal(const QString &key, const QString &path,
                       const QString &interface, const QString &signal,
                       const char *slot);
  /// @param connecting Pending-activation flag; notify on change. No return value.
  void setConnecting(bool connecting);
  /// Deduplicate cached APs by SSID/strength and notify changed rows. No params/return.
  void rebuildNetworks();
  /// Get candidate devices then inspect types asynchronously. No parameters/return.
  void discoverWifiDevice();
  /// Search candidates sequentially, then subscribe/load/scan a Wi-Fi device.
  /// @param devices Paths to inspect. @param index Next candidate.
  /// @param generation Current scan token. No return; exhaustion reports no adapter.
  void findWifiDevice(const QList<QDBusObjectPath> &devices, int index,
                      quint64 generation);
  /// @param devicePath Wireless path for checked AP-added/removed matches. No return.
  void subscribeAccessPoints(const QString &devicePath);
  /// @param devicePath Wireless device to scan; options are empty. No return; errors async.
  void requestScan(const QString &devicePath);
  /// Reconcile known AP paths, remove missing entries and fetch each property map.
  /// @param devicePath Wireless path. @param generation Scan token. No return value.
  void refreshAccessPoints(const QString &devicePath, quint64 generation);
  /// @param apPath Still-known AP to read. @param generation Scan token.
  /// No return; stale/removed AP completions are ignored.
  void fetchAccessPoint(const QString &apPath, quint64 generation);
  /// Read actual SSID/strength and finish only a matching selected-network activation.
  /// @param apPath Active AP path. No return; a different SSID fails the attempt.
  void fetchAccessPointState(const QString &apPath);
  /// Replace subscriptions/status read for a new path, or clear them for an empty path.
  /// @param path Wi-Fi device path. No return value.
  void setWifiDevicePath(const QString &path);
  /// Stop deadline and clear requested SSID/pending state. @param message Failure text.
  /// No return value; visible error is retained for the dialog.
  void failConnection(const QString &message);
  /// @param operation User action label. @param error Raw D-Bus diagnostic.
  /// @return Shared classified display text.
  static QString dbusErrorText(const QString &operation, const QString &error);
  /// Change-notify three independently cached status properties. No return value.
  /// @param connected Activation flag. @param ssid Network name. @param signalStrength Percent.
  void setConnectedState(bool connected, const QString &ssid, int signalStrength);

  QDBusConnection m_bus;
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
  QMap<QString, QString> m_backgroundErrors;
  int m_signalStrength = 0;

private slots:
  /// @param path Added AP; mark known and fetch current-scan properties. No return.
  void onAccessPointAdded(const QDBusObjectPath &path);
  /// @param path Removed AP; clear known/cache entries and rebuild. No return value.
  void onAccessPointRemoved(const QDBusObjectPath &path);
  /// Redetect Wi-Fi on NM root changes only while no device is known. No return.
  /// @param interface Signal interface. @param changedProperties Unused payload.
  /// @param invalidatedProperties Unused key list.
  void onPropertiesChanged(const QString &interface,
                            const QVariantMap &changedProperties,
                            const QStringList &invalidatedProperties);
  /// Refresh AP inventory/status on LastScan/ActiveAccessPoint changes. No return.
  /// @param interface Signal interface. @param changedProperties Changed dictionary.
  /// @param invalidatedProperties Keys requiring fresh lookup.
  void onWifiPropertiesChanged(const QString &interface,
                               const QVariantMap &changedProperties,
                               const QStringList &invalidatedProperties);
  /// Apply Failed/Disconnected/Activated states; other values currently leave status alone.
  /// @param newState NM numeric state. @param oldState Unused previous state.
  /// @param reason Numeric failure detail. No return; Activated resolves active AP.
  void onWifiDeviceStateChanged(uint newState, uint oldState, uint reason);
};
