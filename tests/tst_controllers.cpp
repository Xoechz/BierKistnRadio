#include "ArtCache.h"
#include "BluetoothClient.h"
#include "PlaybackController.h"
#include "SpotifyClient.h"
#include "VolumeController.h"
#include "WifiController.h"
#include <QDBusObjectPath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>
#include <functional>

class TestControllers : public QObject {
  Q_OBJECT

private slots:
  void testPlaybackControllerDefaults();
  void testPlaybackControllerBluetoothTransitions();
  void testWifiControllerDefaults();
  void testWifiControllerScanFindsWifiDevice();
  void testWifiControllerScanIssuesRequestScanOnWireless();
  void testWifiControllerScanNoWifiDevice();
  void testWifiControllerAccessPointAddedRemoved();
  void testWifiControllerConnectIssuesAddAndConnect();
  void testWifiControllerConnectOpenNetworkNoSecurity();
  void testWifiControllerDefaultTracksDisconnected();
  void testWifiControllerTracksActiveConnectionState();
  void testWifiControllerSurfacesConnectError();
  void testWifiControllerStaticHelpers();
  void testBluetoothClientDefaults();
  void testBluetoothTracksConnectedDevices();
  void testBluetoothTakeoverDetection();
  void testBluetoothTakeoverExposesNames();
  void testBluetoothAdapterStateObserved();
  void testBluetoothEnsureDiscoverableCallsSet();
  void testBluetoothResolveTakeoverKeepDisconnectsNew();
  void testBluetoothResolveTakeoverSwitchDisconnectsOld();
  void testBluetoothAvrcpStateFromPlayer();
  void testBluetoothAvrcpOnlyStatusNoTrack();
  void testBluetoothTransportTargetsActiveDevice();
  void testBluetoothMuteDiscoversNodeAndMutes();
  void testBluetoothUnmuteIssuesSetMuteZero();
  void testBluetoothNodeIdFromPwDump();
  void testBluetoothAvrcpResetsOnDeviceChange();
  void testSpotifyClientDefaults();
  void testVolumeControllerDefaults();
  void testVolumeControllerParse();
  void testVolumeControllerReadsFromWpctl();
  void testVolumeControllerPollsExternalChanges();
  void testVolumeControllerIssuesSetVolumeCommand();
  void testVolumeControllerClamping();
  void testVolumeControllerNoReadBackRace();
  void testArtCacheDirCreation();
};

void TestControllers::testPlaybackControllerDefaults() {
  PlaybackController c;
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyUnavailable);
  QCOMPARE(c.isBluetoothActive(), false);
  QVERIFY(c.spotify() != nullptr);
  QVERIFY(c.bluetooth() != nullptr);
}

void TestControllers::testPlaybackControllerBluetoothTransitions() {
  PlaybackController c;
  BluetoothClient *bt = c.bluetooth();
  SpotifyClient *sp = c.spotify();

  // --- A. switchToBluetooth with no device → BluetoothWaiting ---
  bt->setConnectedDeviceNameForTest("");
  c.switchToBluetooth();
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothWaiting);
  QCOMPARE(c.isBluetoothActive(), false);

  // --- B. connect while BluetoothWaiting → BluetoothActive, not muted ---
  bt->setConnectedDeviceNameForTest("Elias S25 FE");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothActive);
  QCOMPARE(c.isBluetoothActive(), true);
  QCOMPARE(bt->muted(), false);

  // --- C. switchToBluetooth with a device already connected → BluetoothActive ---
  bt->setConnectedDeviceNameForTest("");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothWaiting);
  bt->setConnectedDeviceNameForTest("Elias S25 FE");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothActive);
  c.switchToBluetooth();
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothActive);

  // --- D. disconnect while BluetoothActive, no other device → BluetoothWaiting ---
  bt->setConnectedDeviceNameForTest("");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothWaiting);

  // --- E. another device still connected → stays BluetoothActive ---
  bt->setConnectedDeviceNameForTest("Device A");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothActive);
  bt->setConnectedDeviceNameForTest("Device B");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothActive);

  // --- F. connect while in a Spotify state → muted, source unchanged ---
  sp->setAvailableForTest(true);
  sp->setHasTrackForTest(true);
  c.switchToSpotify();
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyActive);
  bt->setConnectedDeviceNameForTest("New Phone");
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyActive); // no switch
  QCOMPARE(bt->muted(), true); // ADR 0006 mute invariant
  QCOMPARE(c.isBluetoothActive(), false);

  // --- G. disconnect while in a Spotify state → no change ---
  bt->setConnectedDeviceNameForTest("");
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyActive); // untouched

  // --- H. switchToSpotify while BluetoothActive → mutes BT stream ---
  bt->setConnectedDeviceNameForTest("");
  c.switchToBluetooth();
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothWaiting);
  bt->setConnectedDeviceNameForTest("Elias S25 FE");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothActive);
  QCOMPARE(bt->muted(), false);

  sp->setAvailableForTest(true);
  sp->setHasTrackForTest(true);
  c.switchToSpotify();
  QCOMPARE(bt->muted(), true);   // mute before pause
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyActive);
  QCOMPARE(c.isBluetoothActive(), false);
}

void TestControllers::testWifiControllerDefaults() {
  WifiController c;
  QCOMPARE(c.connected(), false);
  QCOMPARE(c.ssid(), QString());
  QCOMPARE(c.signalStrength(), 0);
  QCOMPARE(c.errorMessage(), QString());
  QCOMPARE(c.networks(), QVariantList());
}

void TestControllers::testWifiControllerStaticHelpers() {
  // SSID byte array parsing.
  QCOMPARE(WifiController::ssidFromVariant(QVariant(QByteArray("MyNet"))),
           QStringLiteral("MyNet"));
  QCOMPARE(WifiController::ssidFromVariant(QVariant(QStringLiteral("Direct"))),
           QStringLiteral("Direct"));
  QCOMPARE(WifiController::ssidFromVariant(QVariant()), QString());

  // Secured detection from a{sv} props.
  QVERIFY(WifiController::accessPointSecured(
      {{QStringLiteral("WpaFlags"), QVariant(0x00000008)}}));
  QVERIFY(WifiController::accessPointSecured(
      {{QStringLiteral("RsnFlags"), QVariant(0x00000008)}}));
  QVERIFY(WifiController::accessPointSecured(
      {{QStringLiteral("Flags"), QVariant(0x00000001)}})); // privacy flag
  QVERIFY(!WifiController::accessPointSecured(QVariantMap()));

  // Strength extraction.
  QCOMPARE(WifiController::accessPointStrength(
               {{QStringLiteral("Strength"), QVariant(62)}}),
           62);

  // Connection profile dict (secured WPA-PSK network).
  const QString ssid = QStringLiteral("MyWifi");
  const QString password = QStringLiteral("hunter2");
  const QVariantMap profile =
      WifiController::connectionSettings(ssid, password, true);
  const QVariantMap conn = profile[QStringLiteral("connection")].toMap();
  QCOMPARE(conn[QStringLiteral("type")].toString(),
           QStringLiteral("802-11-wireless"));
  QCOMPARE(conn[QStringLiteral("id")].toString(), ssid);
  const QVariantMap wireless = profile[QStringLiteral("802-11-wireless")].toMap();
  QCOMPARE(wireless[QStringLiteral("ssid")].toByteArray(), ssid.toUtf8());
  QCOMPARE(wireless[QStringLiteral("security")].toString(),
           QStringLiteral("802-11-wireless-security"));
  const QVariantMap security =
      profile[QStringLiteral("802-11-wireless-security")].toMap();
  QCOMPARE(security[QStringLiteral("key-mgmt")].toString(),
           QStringLiteral("wpa-psk"));
  QCOMPARE(security[QStringLiteral("psk")].toString(), password);
}

void TestControllers::testWifiControllerScanFindsWifiDevice() {
  WifiController c;
  QString foundDevicePath;
  c.setDbusCallableForTest(
      [&foundDevicePath](const QString &, const QString &objectPath,
                         const QString &interface, const QString &method,
                         const QVariantList &args,
                         const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.DBus.Properties") &&
            method == QStringLiteral("Get")) {
          const QString prop = args.value(1).toString();
          if (prop == QStringLiteral("DeviceType")) {
            QVariant type;
            if (objectPath.endsWith(QStringLiteral("/0")))
              type = QVariant(1); // ethernet
            else if (objectPath.endsWith(QStringLiteral("/1")))
              type = QVariant(2); // wifi
            onFinished(type, QString());
          }
          return;
        }
        if (method == QStringLiteral("GetDevices")) {
          onFinished(QVariant::fromValue(
                         QList<QDBusObjectPath>{QDBusObjectPath(
                                                    QStringLiteral("/org/freedesktop/NetworkManager/Devices/0")),
                                                QDBusObjectPath(
                                                    QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))}),
                     QString());
          return;
        }
        foundDevicePath = objectPath; // RequestScan on the wifi device
        onFinished(QVariant(), QString());
      });
  c.scan();
  QVERIFY(!foundDevicePath.isEmpty());
  QVERIFY(foundDevicePath.endsWith(QStringLiteral("/1")));
}

void TestControllers::testWifiControllerScanIssuesRequestScanOnWireless() {
  WifiController c;
  bool scanOnWirelessInterface = false;
  c.setDbusCallableForTest(
      [&scanOnWirelessInterface](
          const QString &, const QString &objectPath, const QString &interface,
          const QString &method, const QVariantList &,
          const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless") &&
            method == QStringLiteral("RequestScan")) {
          scanOnWirelessInterface = true;
          onFinished(QVariant(), QString());
          return;
        }
        if (interface == QStringLiteral("org.freedesktop.DBus.Properties") &&
            method == QStringLiteral("Get") &&
            objectPath.endsWith(QStringLiteral("/WifiDevice"))) {
          onFinished(QVariant(2), QString());
          return;
        }
        if (method == QStringLiteral("GetDevices")) {
          onFinished(QVariant::fromValue(QList<QDBusObjectPath>{
                          QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/Devices/WifiDevice"))}),
                     QString());
          return;
        }
        onFinished(QVariant(), QString());
      });
  c.scan();
  QVERIFY2(scanOnWirelessInterface,
           "scan() must issue RequestScan on the Wireless interface of the wifi device");
}

void TestControllers::testWifiControllerScanNoWifiDevice() {
  WifiController c;
  c.setDbusCallableForTest(
      [](const QString &, const QString &objectPath, const QString &interface,
         const QString &method, const QVariantList &args,
         const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (method == QStringLiteral("GetDevices")) {
          onFinished(QVariant::fromValue(QList<QDBusObjectPath>{
                          QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/Devices/eth0"))}),
                     QString());
          return;
        }
        if (interface == QStringLiteral("org.freedesktop.DBus.Properties") &&
            args.value(1).toString() == QStringLiteral("DeviceType")) {
          onFinished(QVariant(1), QString()); // ethernet only, no wifi
          return;
        }
        onFinished(QVariant(), QString());
      });
  c.scan();
  QCOMPARE(c.networks(), QVariantList());
}

void TestControllers::testWifiControllerAccessPointAddedRemoved() {
  WifiController c;
  const QString apPath = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/42");
  const QVariantMap apProps{
      {QStringLiteral("Ssid"), QVariant(QByteArray("MyNet"))},
      {QStringLiteral("Strength"), QVariant(70)},
      {QStringLiteral("Flags"), QVariant(0x00000001)},
  };
  c.accessPointAddedForTest(apPath, apProps);
  QCOMPARE(c.networks().size(), 1);
  const QVariantMap ap = c.networks().first().toMap();
  QCOMPARE(ap[QStringLiteral("ssid")].toString(), QStringLiteral("MyNet"));
  QCOMPARE(ap[QStringLiteral("signalStrength")].toInt(), 70);
  QVERIFY(ap[QStringLiteral("secured")].toBool());

  // Remove it again -> list empties.
  c.accessPointRemovedForTest(apPath);
  QCOMPARE(c.networks(), QVariantList());
}

void TestControllers::testWifiControllerConnectIssuesAddAndConnect() {
  WifiController c;
  bool addAndConnectCalled = false;
  QVariantMap capturedSettings;
  c.setDbusCallableForTest(
      [&addAndConnectCalled, &capturedSettings](
          const QString &, const QString &objectPath, const QString &interface,
          const QString &method, const QVariantList &args,
          const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.NetworkManager.Settings") &&
            method == QStringLiteral("AddAndConnectConnection")) {
          addAndConnectCalled = true;
          capturedSettings = args.value(0).toMap();
          onFinished(QVariant(), QString());
          return;
        }
        onFinished(QVariant(), QString());
      });

  c.connect(QStringLiteral("MyNet"), QStringLiteral("hunter2"));
  QVERIFY2(addAndConnectCalled,
           "connect() must call AddAndConnectConnection on Settings");
  QVERIFY(!capturedSettings.isEmpty());
  const QVariantMap conn = capturedSettings[QStringLiteral("connection")].toMap();
  QCOMPARE(conn[QStringLiteral("type")].toString(), QStringLiteral("802-11-wireless"));
  QCOMPARE(capturedSettings[QStringLiteral("802-11-wireless")].toMap()[QStringLiteral("ssid")].toByteArray(),
           QByteArray("MyNet"));
}

void TestControllers::testWifiControllerConnectOpenNetworkNoSecurity() {
  WifiController c;
  QVariantMap capturedSettings;
  c.setDbusCallableForTest(
      [&capturedSettings](const QString &, const QString &, const QString &interface,
                          const QString &method, const QVariantList &args,
                          const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.NetworkManager.Settings") &&
            method == QStringLiteral("AddAndConnectConnection")) {
          capturedSettings = args.value(0).toMap();
        }
        onFinished(QVariant(), QString());
      });
  c.accessPointAddedForTest(QStringLiteral("/ap1"),
                            {{QStringLiteral("Ssid"), QVariant(QByteArray("OpenNet"))},
                             {QStringLiteral("Strength"), QVariant(90)},
                             {QStringLiteral("Flags"), QVariant(0x00000000)}});
  c.connect(QStringLiteral("OpenNet"), QString());
  QVERIFY(!capturedSettings.isEmpty());
  const QVariantMap wireless = capturedSettings[QStringLiteral("802-11-wireless")].toMap();
  QVERIFY(!wireless.contains(QStringLiteral("security")));
  QVERIFY(!capturedSettings.contains(QStringLiteral("802-11-wireless-security")));
}

void TestControllers::testWifiControllerDefaultTracksDisconnected() {
  WifiController c;
  QCOMPARE(c.connected(), false);
  QCOMPARE(c.ssid(), QString());
  QCOMPARE(c.signalStrength(), 0);
}

void TestControllers::testWifiControllerTracksActiveConnectionState() {
  WifiController c;
  const QString activeConnPath = QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/3");
  const QString apPath = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/7");

  c.setDbusCallableForTest(
      [activeConnPath, apPath](const QString &, const QString &objectPath,
                               const QString &interface, const QString &method,
                               const QVariantList &args,
                               const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.NetworkManager") &&
            method == QStringLiteral("GetPrimaryConnection")) {
          onFinished(QVariant::fromValue(QDBusObjectPath(activeConnPath)), QString());
          return;
        }
        // Property reads go through org.freedesktop.DBus.Properties.Get; the
        // target interface is args[0].
        if (interface == QStringLiteral("org.freedesktop.DBus.Properties") &&
            method == QStringLiteral("Get")) {
          const QString targetInterface = args.value(0).toString();
          const QString prop = args.value(1).toString();
          if (targetInterface == QStringLiteral("org.freedesktop.NetworkManager.Connection.Active") &&
              prop == QStringLiteral("SpecificObject")) {
            onFinished(QVariant::fromValue(QDBusObjectPath(apPath)), QString());
            return;
          }
          if (targetInterface == QStringLiteral("org.freedesktop.NetworkManager.AccessPoint")) {
            if (prop == QStringLiteral("Ssid")) {
              onFinished(QVariant(QByteArray("MyNet")), QString());
              return;
            }
            if (prop == QStringLiteral("Strength")) {
              onFinished(QVariant(88), QString());
              return;
            }
          }
        }
        onFinished(QVariant(), QString());
      });
  c.refreshActiveConnection();
  QCOMPARE(c.connected(), true);
  QCOMPARE(c.ssid(), QStringLiteral("MyNet"));
  QCOMPARE(c.signalStrength(), 88);
}

void TestControllers::testWifiControllerSurfacesConnectError() {
  WifiController c;
  const QString accessDenied = QStringLiteral("org.freedesktop.DBus.Error.AccessDenied");
  c.setDbusCallableForTest(
      [accessDenied](const QString &, const QString &, const QString &interface,
                     const QString &method, const QVariantList &,
                     const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.NetworkManager.Settings") &&
            method == QStringLiteral("AddAndConnectConnection")) {
          onFinished(QVariant(), accessDenied);
          return;
        }
        onFinished(QVariant(), QString());
      });
  c.connect(QStringLiteral("MyNet"), QStringLiteral("hunter2"));
  QVERIFY(!c.errorMessage().isEmpty());
  QVERIFY(!c.connected());
}

void TestControllers::testBluetoothClientDefaults() {
  BluetoothClient c;
  QCOMPARE(c.connectedDeviceName(), QString());
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.statusPublished(), false);
  QCOMPARE(c.trackPublished(), false);
  QCOMPARE(c.muted(), false);
}

void TestControllers::testBluetoothTracksConnectedDevices() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  const QVariantMap props{{QStringLiteral("Name"), QStringLiteral("Elias S25 FE")},
                          {QStringLiteral("Alias"), QStringLiteral("Elias")},
                          {QStringLiteral("Connected"), QVariant(true)}};
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"), props);
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("Elias"));
  QCOMPARE(c.takeoverPending(), false);

  // Disconnect -> no active device.
  c.bluezPropertyChangedForTest(dA, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Connected"), QVariant(false)}});
  QCOMPARE(c.connectedDeviceName(), QString());
}

void TestControllers::testBluetoothTakeoverDetection() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  const QString dB = QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  QCOMPARE(c.takeoverPending(), false);

  // Second device connects while one is active -> takeover pending.
  c.bluezObjectAddedForTest(dB, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("B")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  QCOMPARE(c.takeoverPending(), true);
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("A")); // active stays A

  // One disconnects -> takeover resolves back to false.
  c.bluezPropertyChangedForTest(dB, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Connected"), QVariant(false)}});
  QCOMPARE(c.takeoverPending(), false);
}

void TestControllers::testBluetoothTakeoverExposesNames() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  const QString dB = QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  // Single active device: no incoming to name.
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.takeoverIncomingName(), QString());

  // Second device connects: current stays A, incoming is B.
  c.bluezObjectAddedForTest(dB, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("B")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  QCOMPARE(c.takeoverPending(), true);
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("A"));
  QCOMPARE(c.takeoverIncomingName(), QStringLiteral("B"));

  // Resolving clears the incoming-name surface (dialog is done).
  c.resolveTakeover(BluetoothClient::KeepCurrent);
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.takeoverIncomingName(), QString());
}

void TestControllers::testBluetoothAdapterStateObserved() {
  BluetoothClient c;
  const QString adapter = QStringLiteral("/org/bluez/hci0");
  // Defaults: adapter not powered/discoverable/pairable until observed.
  QCOMPARE(c.adapterPowered(), false);
  QCOMPARE(c.adapterDiscoverable(), false);
  QCOMPARE(c.adapterPairable(), false);

  c.bluezObjectAddedForTest(adapter, QStringLiteral("org.bluez.Adapter1"),
                            {{QStringLiteral("Powered"), QVariant(true)},
                             {QStringLiteral("Discoverable"), QVariant(true)},
                             {QStringLiteral("Pairable"), QVariant(true)}});
  QCOMPARE(c.adapterPowered(), true);
  QCOMPARE(c.adapterDiscoverable(), true);
  QCOMPARE(c.adapterPairable(), true);

  // Observe a later change (BlueZ drops Discoverable on connect).
  c.bluezPropertyChangedForTest(adapter, QStringLiteral("org.bluez.Adapter1"),
                                {{QStringLiteral("Discoverable"), QVariant(false)}});
  QCOMPARE(c.adapterPowered(), true);
  QCOMPARE(c.adapterDiscoverable(), false);
  QCOMPARE(c.adapterPairable(), true);
}

void TestControllers::testBluetoothEnsureDiscoverableCallsSet() {
  BluetoothClient c;
  bool setCalled = false;
  c.setDbusCallableForTest(
      [&setCalled](const QString &, const QString &objectPath,
                   const QString &interface, const QString &method,
                   const QVariantList &args,
                   const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.bluez.Adapter1") &&
            method == QStringLiteral("Set")) {
          setCalled = true;
          Q_UNUSED(objectPath);
          QVERIFY(args.value(0).toString() == QStringLiteral("Discoverable"));
          QVERIFY(args.value(1).canConvert<bool>());
          QVERIFY(args.value(1).toBool());
          (void)objectPath;
        }
        onFinished(QVariant(), QString());
      });
  c.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0"),
                            QStringLiteral("org.bluez.Adapter1"), QVariantMap());
  c.ensureDiscoverable();
  QVERIFY2(setCalled, "ensureDiscoverable() must call Adapter1.Set(Discoverable, true)");
}

void TestControllers::testBluetoothResolveTakeoverKeepDisconnectsNew() {
  BluetoothClient c;
  QStringList disconnected;
  c.setDbusCallableForTest(
      [&disconnected](const QString &, const QString &objectPath,
                      const QString &interface, const QString &method,
                      const QVariantList &,
                      const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.bluez.Device1") &&
            method == QStringLiteral("Disconnect")) {
          disconnected.append(objectPath);
        }
        onFinished(QVariant(), QString());
      });
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  const QString dB = QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  c.bluezObjectAddedForTest(dB, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("B")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  QCOMPARE(c.takeoverPending(), true);

  c.resolveTakeover(BluetoothClient::KeepCurrent);
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(disconnected.size(), 1);
  QVERIFY(disconnected.contains(dB)); // the *new* device is kicked
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("A"));
}

void TestControllers::testBluetoothResolveTakeoverSwitchDisconnectsOld() {
  BluetoothClient c;
  QStringList disconnected;
  c.setDbusCallableForTest(
      [&disconnected](const QString &, const QString &objectPath,
                      const QString &interface, const QString &method,
                      const QVariantList &,
                      const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.bluez.Device1") &&
            method == QStringLiteral("Disconnect")) {
          disconnected.append(objectPath);
        }
        onFinished(QVariant(), QString());
      });
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  const QString dB = QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  c.bluezObjectAddedForTest(dB, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("B")},
                             {QStringLiteral("Connected"), QVariant(true)}});

  c.resolveTakeover(BluetoothClient::SwitchToNew);
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(disconnected.size(), 1);
  QVERIFY(disconnected.contains(dA)); // the *old* device is kicked
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("B"));
}

void TestControllers::testBluetoothAvrcpStateFromPlayer() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});

  const QVariantMap track{{QStringLiteral("Title"), QStringLiteral("Stormlight")},
                          {QStringLiteral("Artist"), QStringLiteral("Night")},
                          {QStringLiteral("Album"), QStringLiteral("Flux")},
                          {QStringLiteral("Duration"), QVariant(200000u)}};
  c.bluezObjectAddedForTest(dA + QStringLiteral("/player0"),
                            QStringLiteral("org.bluez.MediaPlayer1"),
                            {{QStringLiteral("Status"), QStringLiteral("playing")},
                             {QStringLiteral("Track"), track},
                             {QStringLiteral("Position"), QVariant(30000u)}});
  QCOMPARE(c.statusPublished(), true);
  QCOMPARE(c.isBluetoothPlaying(), true);
  QCOMPARE(c.trackPublished(), true);
  QCOMPARE(c.trackTitle(), QStringLiteral("Stormlight"));
  QCOMPARE(c.trackArtist(), QStringLiteral("Night"));
  QCOMPARE(c.trackAlbum(), QStringLiteral("Flux"));
  QCOMPARE(c.duration(), qint64(200000));
  QCOMPARE(c.positionPublished(), true);
  QCOMPARE(c.position(), qint64(30000));

  // Position updates without re-publishing the whole track.
  c.bluezPropertyChangedForTest(dA + QStringLiteral("/player0"),
                                QStringLiteral("org.bluez.MediaPlayer1"),
                                {{QStringLiteral("Position"), QVariant(45000u)}});
  QCOMPARE(c.position(), qint64(45000));
}

void TestControllers::testBluetoothAvrcpOnlyStatusNoTrack() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  c.bluezObjectAddedForTest(dA + QStringLiteral("/player0"),
                            QStringLiteral("org.bluez.MediaPlayer1"),
                            {{QStringLiteral("Status"), QStringLiteral("paused")}});
  QCOMPARE(c.statusPublished(), true);
  QCOMPARE(c.isBluetoothPlaying(), false);
  QCOMPARE(c.trackPublished(), false);
  QCOMPARE(c.trackTitle(), QString());
  QCOMPARE(c.positionPublished(), false);
}

void TestControllers::testBluetoothTransportTargetsActiveDevice() {
  BluetoothClient c;
  QString capturedMethod;
  QString capturedPath;
  c.setDbusCallableForTest(
      [&capturedMethod, &capturedPath](
          const QString &, const QString &objectPath, const QString &interface,
          const QString &method, const QVariantList &,
          const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.bluez.MediaPlayer1")) {
          capturedMethod = method;
          capturedPath = objectPath;
        }
        onFinished(QVariant(), QString());
      });
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  c.bluezObjectAddedForTest(dA + QStringLiteral("/player0"),
                            QStringLiteral("org.bluez.MediaPlayer1"),
                            {{QStringLiteral("Status"), QStringLiteral("playing")}});

  c.play();
  QCOMPARE(capturedMethod, QStringLiteral("Play"));
  QCOMPARE(capturedPath, dA + QStringLiteral("/player0"));
  c.pause();
  QCOMPARE(capturedMethod, QStringLiteral("Pause"));
  c.next();
  QCOMPARE(capturedMethod, QStringLiteral("Next"));
  c.previous();
  QCOMPARE(capturedMethod, QStringLiteral("Previous"));
}

void TestControllers::testBluetoothMuteDiscoversNodeAndMutes() {
  BluetoothClient c;
  const QString address = QStringLiteral("AA:BB:CC:DD:EE:FF");
  const QByteArray pwDump =
      "[{\"id\":35,\"type\":\"PipeWire:Interface:Node\",\"info\":{\"props\":"
      "{\"api.bluez5.address\":\"AA:BB:CC:DD:EE:FF\",\"node.name\":"
      "\"bluez_output.AA_BB_CC_DD_EE_FF.a2dp-sink\"}}},{\"id\":41,"
      "\"type\":\"PipeWire:Interface:Node\",\"info\":{\"props\":{\"node.name\":"
      "\"alsa_output.platform-soc_audio.analog-stereo\"}}}]";
  c.setDbusCallableForTest(
      [](const QString &, const QString &, const QString &, const QString &,
         const QVariantList &,
         const std::function<void(const QVariant &, const QString &)> &onFinished) {
        onFinished(QVariant(), QString());
      });
  QStringList calls;
  c.setCommandRunnerForTest(
      [&calls, pwDump](const QStringList &args,
                       const std::function<void(const QByteArray &)> &onFinished) {
        calls.append(args.join(QStringLiteral(" ")));
        if (args.first() == QStringLiteral("pw-dump")) {
          onFinished(pwDump);
        } else {
          onFinished(QByteArray());
        }
      });
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Address"), address},
                             {QStringLiteral("Connected"), QVariant(true)}});

  c.setMuted(true);
  QCOMPARE(c.muted(), true);
  QCOMPARE(calls.size(), 2);
  QCOMPARE(calls, (QStringList{"pw-dump",
                               QStringLiteral("wpctl set-mute 35 1")}));
}

void TestControllers::testBluetoothUnmuteIssuesSetMuteZero() {
  BluetoothClient c;
  const QString address = QStringLiteral("AA:BB:CC:DD:EE:FF");
  const QByteArray pwDump =
      "[{\"id\":35,\"type\":\"PipeWire:Interface:Node\",\"info\":{\"props\":"
      "{\"api.bluez5.address\":\"AA:BB:CC:DD:EE:FF\"}}}]";
  c.setDbusCallableForTest(
      [](const QString &, const QString &, const QString &, const QString &,
         const QVariantList &,
         const std::function<void(const QVariant &, const QString &)> &onFinished) {
        onFinished(QVariant(), QString());
      });
  QStringList calls;
  c.setCommandRunnerForTest(
      [&calls, pwDump](const QStringList &args,
                       const std::function<void(const QByteArray &)> &onFinished) {
        calls.append(args.join(QStringLiteral(" ")));
        if (args.first() == QStringLiteral("pw-dump")) {
          onFinished(pwDump);
        } else {
          onFinished(QByteArray());
        }
      });
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Address"), address},
                             {QStringLiteral("Connected"), QVariant(true)}});

  c.setMuted(false);
  QCOMPARE(c.muted(), false);
  QCOMPARE(calls.size(), 2);
  QCOMPARE(calls.last(), QStringLiteral("wpctl set-mute 35 0"));
}

void TestControllers::testBluetoothNodeIdFromPwDump() {
  const QString address = QStringLiteral("AA:BB:CC:DD:EE:FF");
  const QByteArray dump =
      "[{\"id\":35,\"type\":\"PipeWire:Interface:Node\",\"info\":{\"props\":"
      "{\"api.bluez5.address\":\"AA:BB:CC:DD:EE:FF\",\"node.name\":"
      "\"bluez_output.AA_BB_CC_DD_EE_FF.a2dp-sink\"}}},{\"id\":41,"
      "\"type\":\"PipeWire:Interface:Node\",\"info\":{\"props\":{\"node.name\":"
      "\"alsa_output.platform-soc_audio.analog-stereo\"}}}]";
  QCOMPARE(BluetoothClient::bluetoothNodeIdFromPwDump(dump, address), 35);
  QCOMPARE(BluetoothClient::bluetoothNodeIdFromPwDump(
               dump, QStringLiteral("00:00:00:00:00:00")),
           -1);
  QCOMPARE(BluetoothClient::bluetoothNodeIdFromPwDump(
               QByteArrayLiteral("not json"), address),
           -1);
}

void TestControllers::testBluetoothAvrcpResetsOnDeviceChange() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  c.bluezObjectAddedForTest(dA + QStringLiteral("/player0"),
                            QStringLiteral("org.bluez.MediaPlayer1"),
                            {{QStringLiteral("Status"), QStringLiteral("playing")},
                             {QStringLiteral("Track"),
                              QVariant(QVariantMap{{QStringLiteral("Title"),
                                                     QStringLiteral("Song")}})},
                             {QStringLiteral("Position"), QVariant(100u)}});
  QCOMPARE(c.statusPublished(), true);
  QCOMPARE(c.trackPublished(), true);
  QCOMPARE(c.positionPublished(), true);

  // Device is dropped entirely -> AVRCP state must go quiet (no stale metadata).
  c.bluezObjectRemovedForTest(dA, QStringLiteral("org.bluez.Device1"));
  QCOMPARE(c.connectedDeviceName(), QString());
  QCOMPARE(c.statusPublished(), false);
  QCOMPARE(c.trackPublished(), false);
  QCOMPARE(c.positionPublished(), false);
  QCOMPARE(c.trackTitle(), QString());
}

void TestControllers::testSpotifyClientDefaults() {
  SpotifyClient c;
  QCOMPARE(c.title(), QString());
  QCOMPARE(c.artist(), QString());
  QCOMPARE(c.album(), QString());
  QCOMPARE(c.artUrl(), QString());
  QCOMPARE(c.isSpotifyPlaying(), false);
  QCOMPARE(c.position(), qint64(0));
  QCOMPARE(c.duration(), qint64(0));
  QCOMPARE(c.hasTrack(), false);
  QCOMPARE(c.isAvailable(), false);
}

void TestControllers::testVolumeControllerDefaults() {
  VolumeController c;
  QCOMPARE(c.volume(), 0);
}

void TestControllers::testVolumeControllerParse() {
  QCOMPARE(VolumeController::parseVolume("Volume: 0.65\n"), 65);
  QCOMPARE(VolumeController::parseVolume("Volume: 1.00 [MUTED]\n"), 100);
  QCOMPARE(VolumeController::parseVolume("Volume: 0.00\n"), 0);
  QCOMPARE(VolumeController::parseVolume("Volume: 0.5\n"), 50);
  QCOMPARE(VolumeController::parseVolume("Volume: 2.00\n"), 150);
  QCOMPARE(VolumeController::parseVolume("garbage\n"), -1);
  QCOMPARE(VolumeController::parseVolume(""), -1);
}

void TestControllers::testVolumeControllerReadsFromWpctl() {
  VolumeController c;
  c.setCommandRunnerForTest(
      [](const QStringList &, const std::function<void(const QByteArray &)> &onFinished) {
        onFinished("Volume: 0.65\n");
      });
  c.pollNowForTest();
  QCOMPARE(c.volume(), 65);
}

void TestControllers::testVolumeControllerPollsExternalChanges() {
  VolumeController c;
  QByteArray current("Volume: 0.65\n");
  c.setCommandRunnerForTest(
      [&current](const QStringList &, const std::function<void(const QByteArray &)> &onFinished) {
        onFinished(current);
      });
  c.pollNowForTest();
  QCOMPARE(c.volume(), 65);

  current = "Volume: 0.80\n";
  c.pollNowForTest();
  QCOMPARE(c.volume(), 80);
}

void TestControllers::testVolumeControllerIssuesSetVolumeCommand() {
  VolumeController c;
  QList<QStringList> calls;
  c.setCommandRunnerForTest(
      [&calls](const QStringList &args,
               const std::function<void(const QByteArray &)> &onFinished) {
        calls.append(args);
        onFinished(QByteArray());
      });
  c.setVolume(75);
  QCOMPARE(c.volume(), 75);
  QCOMPARE(calls.size(), 1);
  QCOMPARE(calls.first(), (QStringList{"set-volume", "@DEFAULT_AUDIO_SINK@", "75%"}));
}

void TestControllers::testVolumeControllerClamping() {
  VolumeController c;
  QList<QStringList> calls;
  c.setCommandRunnerForTest(
      [&calls](const QStringList &args,
               const std::function<void(const QByteArray &)> &onFinished) {
        calls.append(args);
        onFinished(QByteArray());
      });
  c.setVolume(200);
  QCOMPARE(c.volume(), 150);
  QCOMPARE(calls.last(), (QStringList{"set-volume", "@DEFAULT_AUDIO_SINK@", "150%"}));

  c.setVolume(-10);
  QCOMPARE(c.volume(), 0);
  QCOMPARE(calls.last(), (QStringList{"set-volume", "@DEFAULT_AUDIO_SINK@", "0%"}));
}

void TestControllers::testVolumeControllerNoReadBackRace() {
  VolumeController c;
  std::function<void(const QByteArray &)> pendingReadFinish;
  QList<QStringList> calls;
  c.setCommandRunnerForTest(
      [&calls, &pendingReadFinish](const QStringList &args,
                                   const std::function<void(const QByteArray &)> &onFinished) {
        calls.append(args);
        if (args.first() == "get-volume") {
          pendingReadFinish = onFinished; // hold the read open (in flight)
        } else {
          onFinished(QByteArray());
        }
      });

  // A poll read is issued and stays in flight...
  c.pollNowForTest();
  QVERIFY(pendingReadFinish);
  QCOMPARE(c.volume(), 0);

  // ...then the user drags the slider before that stale read lands.
  c.setVolume(80);
  QCOMPARE(c.volume(), 80);

  // The stale read completes with the *old* value; must be discarded.
  pendingReadFinish("Volume: 0.50\n");
  QCOMPARE(c.volume(), 80);
}

void TestControllers::testArtCacheDirCreation() {
  ArtCache cache;
  QVERIFY(!cache.cacheDir().isEmpty());
  QVERIFY(QDir(cache.cacheDir()).exists());
}

QTEST_MAIN(TestControllers)
#include "tst_controllers.moc"
