#include "ArtCache.h"
#include "BluetoothClient.h"
#include "PlaybackController.h"
#include "PowerController.h"
#include "SpotifyClient.h"
#include "VolumeController.h"
#include "WifiController.h"
#include <QDBusObjectPath>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVirtualObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QScopeGuard>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest/QtTest>
#include <functional>

class MockBlueZObjects : public QDBusVirtualObject {
public:
  QString lookupError;
  QString playerError;
  bool invalidLookupReply = false;
  QString introspect(const QString &) const override {
    return QStringLiteral("<interface name=\"org.freedesktop.DBus.ObjectManager\">"
                          "<method name=\"GetManagedObjects\">"
                          "<arg direction=\"out\" type=\"a{oa{sa{sv}}}\"/>"
                          "</method></interface>");
  }

  bool handleMessage(const QDBusMessage &message,
                      const QDBusConnection &connection) override {
    if (message.member() == QStringLiteral("GetAll")) {
      if (!playerError.isEmpty()) {
        connection.send(message.createErrorReply(playerError, QStringLiteral("Mock read failure")));
      } else {
        connection.send(message.createReply(QVariantMap{
            {QStringLiteral("Status"), QStringLiteral("playing")},
            {QStringLiteral("Track"), QVariantMap{
                 {QStringLiteral("Title"), QStringLiteral("Initial Track")},
                 {QStringLiteral("Duration"), 80000u}}}}));
      }
      return true;
    }
    if (message.member() != QStringLiteral("GetManagedObjects")) {
      return false;
    }
    if (!lookupError.isEmpty()) {
      connection.send(message.createErrorReply(lookupError, QStringLiteral("Mock lookup failure")));
      return true;
    }
    if (invalidLookupReply) {
      connection.send(message.createReply(QStringLiteral("invalid snapshot")));
      return true;
    }
    QDBusArgument arg;
    arg.beginMap(QMetaType::fromType<QDBusObjectPath>(),
                 QMetaType::fromType<QMap<QString, QVariantMap>>());
    const auto add = [&arg](const QString &path, const QString &interface,
                            const QVariantMap &props) {
      arg.beginMapEntry();
      arg << QDBusObjectPath(path)
          << QMap<QString, QVariantMap>{{interface, props}};
      arg.endMapEntry();
    };
    // Deliberately put the player before its Device1 in the snapshot.
    add(QStringLiteral("/org/bluez/hci0/dev_A/player0"),
        QStringLiteral("org.bluez.MediaPlayer1"),
        {{QStringLiteral("Status"), QStringLiteral("playing")},
         {QStringLiteral("Track"), QVariantMap{{QStringLiteral("Title"),
                                                QStringLiteral("Initial Track")},
                                               {QStringLiteral("Duration"), 80000u}}}});
    add(QStringLiteral("/org/bluez/hci0/dev_A"), QStringLiteral("org.bluez.Device1"),
        {{QStringLiteral("Connected"), true},
         {QStringLiteral("Alias"), QStringLiteral("Phone")}});
    add(QStringLiteral("/org/bluez/hci0"), QStringLiteral("org.bluez.Adapter1"),
        {{QStringLiteral("Powered"), true}});
    arg.endMap();
    connection.send(message.createReply(QVariant::fromValue(arg)));
    return true;
  }
};

class MockMprisPlayer : public QDBusVirtualObject {
public:
  QList<QDBusMessage> calls;
  bool denyCommands = false;
  QString readError;
  bool invalidReadReply = false;
  bool holdReads = false;
  QList<QDBusMessage> pendingReads;
  QVariantMap metadata{{QStringLiteral("xesam:title"), QStringLiteral("Initial Track")},
                       {QStringLiteral("xesam:artist"),
                        QStringList{QStringLiteral("First Artist"),
                                    QStringLiteral("Guest")}},
                       {QStringLiteral("xesam:album"), QStringLiteral("First Album")},
                       {QStringLiteral("mpris:artUrl"),
                        QStringLiteral("https://example.org/first.jpg")},
                       {QStringLiteral("mpris:length"), qint64(190000000)},
                       {QStringLiteral("mpris:trackid"),
                        QVariant::fromValue(QDBusObjectPath(
                            QStringLiteral("/org/mpris/MediaPlayer2/Track/1")))}};

  QString introspect(const QString &) const override {
    return QStringLiteral("<interface name=\"org.freedesktop.DBus.Properties\">"
                          "<method name=\"GetAll\"><arg direction=\"in\" type=\"s\"/>"
                          "<arg direction=\"out\" type=\"a{sv}\"/></method>"
                          "<signal name=\"PropertiesChanged\">"
                          "<arg type=\"s\"/><arg type=\"a{sv}\"/><arg type=\"as\"/>"
                          "</signal></interface>"
                          "<interface name=\"org.mpris.MediaPlayer2.Player\">"
                          "<method name=\"Play\"/><method name=\"Pause\"/>"
                          "<method name=\"Next\"/><method name=\"Previous\"/>"
                          "<method name=\"SetPosition\"><arg direction=\"in\" type=\"o\"/>"
                          "<arg direction=\"in\" type=\"x\"/></method></interface>");
  }

  bool handleMessage(const QDBusMessage &message,
                     const QDBusConnection &connection) override {
    if (message.member() == QStringLiteral("GetAll")) {
      if (holdReads) {
        message.setDelayedReply(true);
        pendingReads.append(message);
        return true;
      }
      if (!readError.isEmpty()) {
        connection.send(message.createErrorReply(readError, QStringLiteral("Mock read failure")));
        return true;
      }
      if (invalidReadReply) {
        connection.send(message.createReply(QStringLiteral("invalid properties")));
        return true;
      }
      connection.send(message.createReply(QVariantMap{
          {QStringLiteral("PlaybackStatus"), QStringLiteral("Playing")},
          {QStringLiteral("Metadata"), metadata},
          {QStringLiteral("Position"), qint64(5000000)}}));
      return true;
    }
    calls.append(message);
    if (denyCommands) {
      connection.send(message.createErrorReply(
          QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
          QStringLiteral("Not authorized")));
    } else {
      connection.send(message.createReply());
    }
    return true;
  }
};

class MockNetworkManager : public QDBusVirtualObject {
public:
  bool scanRequested = false;
  bool activationRequested = false;
  QVariantList activationArgs;

  QString introspect(const QString &) const override {
    return QStringLiteral("<interface name=\"org.freedesktop.NetworkManager\">"
                          "<method name=\"GetDevices\"><arg direction=\"out\" type=\"ao\"/>"
                          "</method><method name=\"AddAndActivateConnection\">"
                          "<arg direction=\"in\" type=\"a{sa{sv}}\"/>"
                          "<arg direction=\"in\" type=\"o\"/>"
                          "<arg direction=\"in\" type=\"o\"/>"
                          "<arg direction=\"out\" type=\"o\"/>"
                          "<arg direction=\"out\" type=\"o\"/></method></interface>"
                          "<interface name=\"org.freedesktop.NetworkManager.Device.Wireless\">"
                          "<method name=\"GetAllAccessPoints\">"
                          "<arg direction=\"out\" type=\"ao\"/></method>"
                          "<method name=\"RequestScan\"><arg direction=\"in\" "
                          "type=\"a{sv}\"/></method></interface>"
                          "<interface name=\"org.freedesktop.DBus.Properties\">"
                          "<method name=\"Get\"><arg direction=\"in\" type=\"s\"/>"
                          "<arg direction=\"in\" type=\"s\"/>"
                          "<arg direction=\"out\" type=\"v\"/></method>"
                          "<method name=\"GetAll\"><arg direction=\"in\" type=\"s\"/>"
                          "<arg direction=\"out\" type=\"a{sv}\"/></method>"
                          "</interface>");
  }

  bool handleMessage(const QDBusMessage &message,
                     const QDBusConnection &connection) override {
    const QString method = message.member();
    const QString managerPath = QStringLiteral("/org/freedesktop/NetworkManager");
    const QString devicePath = managerPath + QStringLiteral("/Devices/1");
    const QString apPath = managerPath + QStringLiteral("/AccessPoint/1");
    const QString properties = QStringLiteral("org.freedesktop.DBus.Properties");
    const QString wireless = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
    const QString nm = QStringLiteral("org.freedesktop.NetworkManager");
    const bool valid =
        (method == QStringLiteral("GetDevices") && message.path() == managerPath &&
         message.interface() == nm && message.signature().isEmpty()) ||
        (method == QStringLiteral("GetAllAccessPoints") && message.path() == devicePath &&
         message.interface() == wireless && message.signature().isEmpty()) ||
        (method == QStringLiteral("RequestScan") && message.path() == devicePath &&
         message.interface() == wireless && message.signature() == QStringLiteral("a{sv}")) ||
        (method == QStringLiteral("AddAndActivateConnection") && message.path() == managerPath &&
         message.interface() == nm && message.signature() == QStringLiteral("a{sa{sv}}oo")) ||
        (method == QStringLiteral("Get") && message.path() == devicePath &&
         message.interface() == properties && message.signature() == QStringLiteral("ss")) ||
        (method == QStringLiteral("GetAll") && message.path() == apPath &&
         message.interface() == properties && message.signature() == QStringLiteral("s"));
    if (!valid) {
      connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
          QStringLiteral("Invalid NetworkManager path, interface or signature")));
      return true;
    }
    if (method == QStringLiteral("GetDevices") ||
        method == QStringLiteral("GetAllAccessPoints")) {
      const QString path = method == QStringLiteral("GetDevices")
                               ? QStringLiteral("/org/freedesktop/NetworkManager/Devices/1")
                               : QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1");
      QDBusArgument array;
      array.beginArray(QMetaType::fromType<QDBusObjectPath>());
      array << QDBusObjectPath(path);
      array.endArray();
      connection.send(message.createReply(QVariant::fromValue(array)));
    } else if (method == QStringLiteral("Get")) {
      const QString prop = message.arguments().value(1).toString();
      QVariant value;
      if (prop == QStringLiteral("DeviceType")) {
        value = QVariant(2u);
      } else if (prop == QStringLiteral("State")) {
        value = QVariant(30u);
      } else if (prop == QStringLiteral("ActiveAccessPoint")) {
        value = QVariant::fromValue(QDBusObjectPath(
            QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1")));
      }
      connection.send(message.createReply(
          QVariant::fromValue(QDBusVariant(value))));
    } else if (method == QStringLiteral("GetAll")) {
      connection.send(message.createReply(QVariantMap{
          {QStringLiteral("Ssid"), QByteArray("PrivateWifi")},
          {QStringLiteral("Strength"), QVariant::fromValue(uchar(71))},
          {QStringLiteral("RsnFlags"), QVariant(8u)}}));
    } else if (method == QStringLiteral("RequestScan")) {
      scanRequested = true;
      connection.send(message.createReply());
    } else if (method == QStringLiteral("AddAndActivateConnection")) {
      activationRequested = true;
      activationArgs = message.arguments();
      connection.send(message.createReply(QVariantList{
          QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/Settings/1"))),
          QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/freedesktop/NetworkManager/ActiveConnection/1")))}));
    } else {
      connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.UnknownMethod"),
                                               QStringLiteral("Unknown mock method")));
    }
    return true;
  }
};

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
  void testWifiControllerListsExistingAccessPoints();
  void testWifiControllerScanError();
  void testWifiControllerActivationResult();
  void testWifiPrivateBusScanAndActivation();
  void testBluetoothMuteProcessTimeout();
  void testBluetoothClientDefaults();
  void testBluetoothTracksConnectedDevices();
  void testBluetoothTakeoverDetection();
  void testBluetoothTakeoverExposesNames();
  void testBluetoothTakeoverIncomingNameLiveUpdate();
  void testBluetoothDeviceRemoveClearsPlayerState();
  void testBluetoothAdapterStateObserved();
  void testBluetoothEnsureDiscoverableCallsSet();
  void testBluetoothDiscoverabilityErrorAndRecovery();
  void testBluetoothTransportErrorAndRecovery();
  void testBluetoothResolveTakeoverKeepDisconnectsNew();
  void testBluetoothResolveTakeoverSwitchDisconnectsOld();
  void testBluetoothTakeoverDisconnectFailureAndRetry();
  void testBluetoothTakeoverIncomingDisappearsDuringDisconnect();
  void testBluetoothTakeoverDisconnectTimeout();
  void testBluetoothAvrcpStateFromPlayer();
  void testBluetoothAvrcpOnlyStatusNoTrack();
  void testBluetoothTransportTargetsActiveDevice();
  void testBluetoothMuteDiscoversNodeAndMutes();
  void testBluetoothUnmuteIssuesSetMuteZero();
  void testBluetoothMuteCoversAllConnectedDevices();
  void testBluetoothMuteCommandFailure();
  void testBluetoothNodeIdFromPwDump();
  void testBluetoothAvrcpResetsOnDeviceChange();
  void testBluetoothRetargetsPlayerAfterTakeover();
  void testBluetoothDeviceWithoutNameIsDetected();
  void testBluetoothPlayerAddedBeforeDevice();
  void testBluetoothPrivateBusObjectManagerAndProperties();
  void testBackgroundDbusReadErrors_data();
  void testBackgroundDbusReadErrors();
  void testDisconnectedBusErrors();
  void testSpotifyClientDefaults();
  void testSpotifyFirstArtistFromMetadata();
  void testSpotifyPrivateBusMprisLifecycle();
  void testSpotifyIgnoresStaleStateReplies();
  void testVolumeControllerDefaults();
  void testVolumeControllerParse();
  void testVolumeControllerReadsFromWpctl();
  void testVolumeControllerPollsExternalChanges();
  void testVolumeControllerIssuesSetVolumeCommand();
  void testVolumeControllerClamping();
  void testVolumeControllerNoReadBackRace();
  void testVolumeCommandErrorsAndRecovery();
  void testPowerCommandErrorsAndRetry();
  void testVolumeMuteRestoresLastLevel();
  void testVolumeMuteFallsBackToTenPercent();
  void testVolumeMuteTracksSliderAndExternalChanges();
  void testArtCacheDirCreation();
  void testArtCacheDownloadReuseAndClear();
  void testArtCachePrunesOldCovers();
  void testArtCacheRejectsFailuresAndInvalidImages();
};

void TestControllers::testPlaybackControllerDefaults() {
  PlaybackController c;
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyWaiting);
  QCOMPARE(c.isBluetoothActive(), false);
  QVERIFY(c.spotify() != nullptr);
  QVERIFY(c.bluetooth() != nullptr);
}

void TestControllers::testPlaybackControllerBluetoothTransitions() {
  // Isolated missing buses: requested-source selection must survive startup
  // failure, and metadata/connection events must not select another source.
  PlaybackController c(QDBusConnection(QStringLiteral("missing-session")),
                       QDBusConnection(QStringLiteral("missing-system")));
  BluetoothClient *bt = c.bluetooth();
  SpotifyClient *sp = c.spotify();
  bt->setConnectedDeviceNameForTest("");
  c.switchToBluetooth();
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothWaiting);
  QTRY_VERIFY(!c.switching());
  QVERIFY(!c.sourceError().isEmpty());
  QVERIFY(!c.sourceReady());
  bt->setConnectedDeviceNameForTest("Elias S25 FE");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothActive);
  QVERIFY(bt->muted()); // a connection is not proof the source is ready
  bt->setConnectedDeviceNameForTest("");
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothWaiting);
  sp->setAvailableForTest(true);
  sp->setHasTrackForTest(true);
  QCOMPARE(c.playbackState(), PlaybackController::BluetoothWaiting);
  c.switchToSpotify();
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyWaiting);
  QTRY_VERIFY(!c.switching());
  bt->setConnectedDeviceNameForTest("New Phone");
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyWaiting);
  QVERIFY(bt->muted());
}

void TestControllers::testWifiControllerDefaults() {
  WifiController c;
  QCOMPARE(c.connected(), false);
  QCOMPARE(c.ssid(), QString());
  QCOMPARE(c.signalStrength(), 0);
  QCOMPARE(c.errorMessage(), QString());
  QCOMPARE(c.connecting(), false);
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
         if (interface == QStringLiteral("org.freedesktop.NetworkManager") &&
             objectPath == QStringLiteral("/org/freedesktop/NetworkManager") &&
             method == QStringLiteral("AddAndActivateConnection")) {
           addAndConnectCalled = true;
           QCOMPARE(args.size(), 3);
           QCOMPARE(args.at(1).value<QDBusObjectPath>().path(),
                    QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"));
           QCOMPARE(args.at(2).value<QDBusObjectPath>().path(), QStringLiteral("/"));
           const auto profile = args.at(0).value<WifiController::SettingsMap>();
           for (auto it = profile.cbegin(); it != profile.cend(); ++it) {
             capturedSettings.insert(it.key(), it.value());
           }
           onFinished(QVariant(), QString());
           return;
         }
         if (method == QStringLiteral("GetDevices")) {
           onFinished(QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(
               QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))}), QString());
           return;
         }
         if (method == QStringLiteral("Get") && args.value(1).toString() ==
                 QStringLiteral("DeviceType")) {
           onFinished(2u, QString());
           return;
         }
         onFinished(QVariant(), QString());
       });

  c.scan();
  c.connect(QStringLiteral("MyNet"), QStringLiteral("hunter2"));
  QVERIFY2(addAndConnectCalled,
            "connect() must call AddAndActivateConnection on the manager");
  QVERIFY(!capturedSettings.isEmpty());
  const QVariantMap conn = capturedSettings[QStringLiteral("connection")].toMap();
  QCOMPARE(conn[QStringLiteral("type")].toString(), QStringLiteral("802-11-wireless"));
  QCOMPARE(capturedSettings[QStringLiteral("802-11-wireless")].toMap()[QStringLiteral("ssid")].toByteArray(),
           QByteArray("MyNet"));
  QCOMPARE(QString::fromLatin1(QDBusMetaType::typeToSignature(
               QMetaType::fromType<WifiController::SettingsMap>())),
           QStringLiteral("a{sa{sv}}"));
}

void TestControllers::testWifiControllerConnectOpenNetworkNoSecurity() {
  WifiController c;
  QVariantMap capturedSettings;
  c.setDbusCallableForTest(
      [&capturedSettings](const QString &, const QString &, const QString &interface,
                          const QString &method, const QVariantList &args,
                          const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.NetworkManager") &&
            method == QStringLiteral("AddAndActivateConnection")) {
          QCOMPARE(args.at(2).value<QDBusObjectPath>().path(), QStringLiteral("/ap1"));
          const auto profile = args.value(0).value<WifiController::SettingsMap>();
          for (auto it = profile.cbegin(); it != profile.cend(); ++it) {
            capturedSettings.insert(it.key(), it.value());
          }
        } else if (method == QStringLiteral("GetDevices")) {
          onFinished(QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(
              QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))}), QString());
          return;
        } else if (method == QStringLiteral("Get") && args.value(1).toString() ==
                       QStringLiteral("DeviceType")) {
          onFinished(2u, QString());
          return;
        }
        onFinished(QVariant(), QString());
      });
  c.scan();
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
  const QString apPath = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/7");

  c.setDbusCallableForTest(
       [apPath](const QString &, const QString &objectPath,
                               const QString &interface, const QString &method,
                               const QVariantList &args,
                               const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.DBus.Properties") &&
            method == QStringLiteral("Get")) {
          const QString targetInterface = args.value(0).toString();
          const QString prop = args.value(1).toString();
          if (prop == QStringLiteral("DeviceType")) {
            onFinished(2u, QString());
            return;
        }
          if (prop == QStringLiteral("State")) {
            onFinished(100u, QString());
            return;
          }
          if (targetInterface == QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless") &&
              prop == QStringLiteral("ActiveAccessPoint")) {
            onFinished(QVariant::fromValue(QDBusObjectPath(apPath)), QString());
            return;
          }
        }
        if (method == QStringLiteral("GetAll") && objectPath == apPath) {
          onFinished(QVariantMap{{QStringLiteral("Ssid"), QByteArray("MyNet")},
                               {QStringLiteral("Strength"), 88}}, QString());
          return;
        }
        if (method == QStringLiteral("GetDevices")) {
          onFinished(QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(
              QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))}), QString());
          return;
          }
        onFinished(QVariant(), QString());
      });
  c.scan();
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
                      const QString &method, const QVariantList &args,
                     const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.NetworkManager") &&
            method == QStringLiteral("AddAndActivateConnection")) {
          onFinished(QVariant(), accessDenied);
          return;
        }
        if (method == QStringLiteral("GetDevices")) {
          onFinished(QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(
              QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))}), QString());
          return;
        }
        if (method == QStringLiteral("Get") && args.value(1).toString() ==
                QStringLiteral("DeviceType")) {
          onFinished(2u, QString());
          return;
        }
        onFinished(QVariant(), QString());
      });
  c.scan();
  c.connect(QStringLiteral("MyNet"), QStringLiteral("hunter2"));
  QCOMPARE(c.errorMessage(), QStringLiteral("Permission denied — check system config"));
  QCOMPARE(c.connecting(), false);
  QVERIFY(!c.connected());
}

void TestControllers::testWifiControllerListsExistingAccessPoints() {
  WifiController c;
  const QString apPath = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/7");
  int subscriptionsToInventory = 0;
  c.setDbusCallableForTest(
      [&subscriptionsToInventory, apPath](
          const QString &, const QString &path, const QString &,
          const QString &method, const QVariantList &args,
          const std::function<void(const QVariant &, const QString &)> &done) {
        if (method == QStringLiteral("GetDevices")) {
          done(QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(
              QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))}), QString());
        } else if (method == QStringLiteral("Get") &&
                   args.value(1).toString() == QStringLiteral("DeviceType")) {
          done(2u, QString());
        } else if (method == QStringLiteral("GetAllAccessPoints")) {
          ++subscriptionsToInventory;
          done(QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(apPath)}),
               QString());
        } else if (method == QStringLiteral("GetAll") && path == apPath) {
          done(QVariantMap{{QStringLiteral("Ssid"), QByteArray("Existing")},
                           {QStringLiteral("Strength"), 75u}}, QString());
        } else {
          done(QVariant(), QString());
        }
      });
  c.scan();
  QCOMPARE(c.networks().size(), 1);
  QCOMPARE(c.networks().first().toMap().value(QStringLiteral("ssid")).toString(),
           QStringLiteral("Existing"));
  const QVariantMap changed{{QStringLiteral("LastScan"), 123}};
  QVERIFY(QMetaObject::invokeMethod(&c, "onWifiPropertiesChanged",
      Qt::DirectConnection,
      Q_ARG(QString, QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless")),
      Q_ARG(QVariantMap, changed), Q_ARG(QStringList, QStringList())));
  QCOMPARE(subscriptionsToInventory, 2);
  QCOMPARE(c.networks().size(), 1); // an unchanged AP is not re-added twice
}

void TestControllers::testWifiControllerScanError() {
  WifiController c;
  QString failure = QStringLiteral("org.freedesktop.DBus.Error.AccessDenied");
  c.setDbusCallableForTest(
      [&failure](const QString &, const QString &, const QString &,
          const QString &method, const QVariantList &,
          const std::function<void(const QVariant &, const QString &)> &done) {
         if (method == QStringLiteral("GetDevices")) {
           done(QVariant(), failure);
         }
       });
  c.scan();
  QCOMPARE(c.errorMessage(), QStringLiteral("Permission denied — check system config"));
  failure = QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown");
  c.scan();
  QCOMPARE(c.errorMessage(),
           QStringLiteral("Wi-Fi device lookup: service unavailable — check system config"));
  failure = QStringLiteral("org.freedesktop.DBus.Error.NoReply");
  c.scan();
  QCOMPARE(c.errorMessage(), QStringLiteral("Wi-Fi device lookup: timed out — try again"));
}

void TestControllers::testWifiControllerActivationResult() {
  WifiController c;
  const QString apPath = QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/7");
  bool differentNetwork = false;
  c.setDbusCallableForTest(
      [&differentNetwork, apPath](const QString &, const QString &path,
                                  const QString &, const QString &method,
                                  const QVariantList &args,
                                  const std::function<void(const QVariant &,
                                                           const QString &)> &done) {
        if (method == QStringLiteral("GetDevices")) {
          done(QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(
              QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"))}), QString());
        } else if (method == QStringLiteral("Get") &&
                   args.value(1).toString() == QStringLiteral("DeviceType")) {
          done(2u, QString());
        } else if (method == QStringLiteral("Get") &&
                   args.value(1).toString() == QStringLiteral("State")) {
          done(30u, QString());
        } else if (method == QStringLiteral("Get") &&
                   args.value(1).toString() == QStringLiteral("ActiveAccessPoint")) {
          done(QVariant::fromValue(QDBusObjectPath(apPath)), QString());
        } else if (method == QStringLiteral("GetAll") && path == apPath) {
          done(QVariantMap{{QStringLiteral("Ssid"),
                            differentNetwork ? QByteArray("Other") : QByteArray("Chosen")},
                           {QStringLiteral("Strength"), 80u}}, QString());
        } else {
          done(QVariant(), QString());
        }
      });
  c.scan();
  QSignalSpy succeeded(&c, &WifiController::connectionSucceeded);
  c.connect(QStringLiteral("Chosen"), QStringLiteral("passphrase"));
  QCOMPARE(succeeded.count(), 0); // method accepted; not yet activated
  QCOMPARE(c.connecting(), true);
  differentNetwork = true;
  QVERIFY(QMetaObject::invokeMethod(&c, "onWifiDeviceStateChanged",
      Qt::DirectConnection, Q_ARG(uint, 100u), Q_ARG(uint, 30u), Q_ARG(uint, 0u)));
  QCOMPARE(succeeded.count(), 0);
  QCOMPARE(c.connecting(), false);
  QVERIFY(c.errorMessage().contains(QStringLiteral("different")));

  differentNetwork = false;
  c.connect(QStringLiteral("Chosen"), QStringLiteral("passphrase"));
  QVERIFY(QMetaObject::invokeMethod(&c, "onWifiDeviceStateChanged",
      Qt::DirectConnection, Q_ARG(uint, 100u), Q_ARG(uint, 30u), Q_ARG(uint, 0u)));
  QCOMPARE(succeeded.count(), 1);
  QCOMPARE(c.connecting(), false);
  QCOMPARE(succeeded.first().first().toString(), QStringLiteral("Chosen"));
  QCOMPARE(c.ssid(), QStringLiteral("Chosen"));

  c.connect(QStringLiteral("Missing"), QStringLiteral("passphrase"));
  QVERIFY(QMetaObject::invokeMethod(&c, "onWifiDeviceStateChanged",
      Qt::DirectConnection, Q_ARG(uint, 120u), Q_ARG(uint, 50u), Q_ARG(uint, 7u)));
  QCOMPARE(succeeded.count(), 1);
  QVERIFY(c.errorMessage().contains(QStringLiteral("reason 7")));
  QCOMPARE(c.connecting(), false);
}

void TestControllers::testWifiPrivateBusScanAndActivation() {
  QProcess daemon;
  daemon.start(QStringLiteral("dbus-daemon"),
               {QStringLiteral("--session"), QStringLiteral("--nofork"),
                QStringLiteral("--print-address=1")});
  QVERIFY(daemon.waitForStarted());
  QVERIFY(daemon.waitForReadyRead(5000));
  const QString address = QString::fromUtf8(daemon.readAllStandardOutput()).trimmed();
  QVERIFY(!address.isEmpty());
  const QString name = QStringLiteral("nm-test-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces));
  QDBusConnection bus = QDBusConnection::connectToBus(address, name);
  const auto cleanup = qScopeGuard([&]() {
    QDBusConnection::disconnectFromBus(name);
    daemon.terminate();
    daemon.waitForFinished(3000);
  });
  QVERIFY(bus.isConnected());
  QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.NetworkManager")));
  MockNetworkManager manager;
  const QString nmPath = QStringLiteral("/org/freedesktop/NetworkManager");
  QVERIFY(bus.registerVirtualObject(nmPath, &manager, QDBusConnection::SubPath));

  {
    WifiController wifi(bus);
    QSignalSpy networksChanged(&wifi, &WifiController::networksChanged);
    QSignalSpy succeeded(&wifi, &WifiController::connectionSucceeded);
    QTRY_COMPARE_WITH_TIMEOUT(wifi.networks().size(), 1, 3000);
    QVERIFY(manager.scanRequested);
    QVERIFY(networksChanged.size() >= 1);
    const QVariantMap network = wifi.networks().first().toMap();
    QCOMPARE(network.value(QStringLiteral("ssid")).toString(),
             QStringLiteral("PrivateWifi"));
    QCOMPARE(network.value(QStringLiteral("signalStrength")).toInt(), 71);
    QVERIFY(network.value(QStringLiteral("secured")).toBool());

    wifi.connect(QStringLiteral("PrivateWifi"), QStringLiteral("test-password"));
    QTRY_VERIFY_WITH_TIMEOUT(manager.activationRequested, 3000);
    QCOMPARE(manager.activationArgs.size(), 3);
    QVERIFY(manager.activationArgs.at(0).canConvert<QDBusArgument>());
    const auto profile = qdbus_cast<WifiController::SettingsMap>(
        manager.activationArgs.at(0).value<QDBusArgument>());
    QCOMPARE(profile.value(QStringLiteral("802-11-wireless"))
                 .value(QStringLiteral("ssid")).toByteArray(),
             QByteArray("PrivateWifi"));
    QCOMPARE(profile.value(QStringLiteral("802-11-wireless-security"))
                 .value(QStringLiteral("psk")).toString(),
             QStringLiteral("test-password"));
    QCOMPARE(manager.activationArgs.at(1).value<QDBusObjectPath>().path(),
             QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"));
    QCOMPARE(manager.activationArgs.at(2).value<QDBusObjectPath>().path(),
             QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/1"));
    QVERIFY(wifi.connecting());
    QCOMPARE(succeeded.size(), 0); // method reply alone is not activation

    QDBusMessage activated = QDBusMessage::createSignal(
        QStringLiteral("/org/freedesktop/NetworkManager/Devices/1"),
        QStringLiteral("org.freedesktop.NetworkManager.Device"),
        QStringLiteral("StateChanged"));
    activated << uint(100) << uint(30) << uint(0);
    QVERIFY(bus.send(activated));
    QTRY_COMPARE_WITH_TIMEOUT(succeeded.size(), 1, 3000);
    QVERIFY(wifi.connected());
    QVERIFY(!wifi.connecting());
    QCOMPARE(wifi.ssid(), QStringLiteral("PrivateWifi"));
    QCOMPARE(wifi.signalStrength(), 71);

    // Daemon loss cancels an in-flight activation, invalidates cached paths,
    // and surfaces an error. Reappearance reloads APs and subscriptions once.
    wifi.connect(QStringLiteral("PrivateWifi"), QStringLiteral("test-password"));
    QVERIFY(wifi.connecting());
    QVERIFY(bus.unregisterService(QStringLiteral("org.freedesktop.NetworkManager")));
    QTRY_VERIFY(!wifi.connecting());
    QTRY_VERIFY(wifi.networks().isEmpty());
    QVERIFY(!wifi.connected());
    QVERIFY(wifi.errorMessage().contains(QStringLiteral("service unavailable")));
    QCOMPARE(succeeded.size(), 1);
    QVERIFY(bus.registerService(QStringLiteral("org.freedesktop.NetworkManager")));
    QTRY_COMPARE(wifi.networks().size(), 1);
    QVERIFY(wifi.errorMessage().isEmpty());
    wifi.connect(QStringLiteral("PrivateWifi"), QStringLiteral("test-password"));
    QVERIFY(bus.send(activated));
    QTRY_COMPARE(succeeded.size(), 2);
    QTest::qWait(100);
    QCOMPARE(succeeded.size(), 2); // no duplicate subscription after recovery
  }
  bus.unregisterObject(nmPath);
  bus.unregisterService(QStringLiteral("org.freedesktop.NetworkManager"));
}

void TestControllers::testBluetoothClientDefaults() {
  BluetoothClient c;
  QCOMPARE(c.connectedDeviceName(), QString());
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.statusPublished(), false);
  QCOMPARE(c.trackPublished(), false);
  QCOMPARE(c.muted(), false);
}

void TestControllers::testBluetoothMuteProcessTimeout() {
  QTemporaryDir commands;
  QVERIFY(commands.isValid());
  QFile dump(commands.filePath(QStringLiteral("pw-dump")));
  QVERIFY(dump.open(QIODevice::WriteOnly));
  dump.write("#!/bin/sh\nexec sleep 30\n");
  dump.close();
  QVERIFY(dump.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  const QByteArray previousPath = qgetenv("PATH");
  const auto restorePath = qScopeGuard([&]() { qputenv("PATH", previousPath); });
  qputenv("PATH", commands.path().toUtf8() + ':' + previousPath);
  BluetoothClient bluetooth(QDBusConnection(QStringLiteral("missing-bluez-process-test")));
  bluetooth.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0/dev_A"),
      QStringLiteral("org.bluez.Device1"), {{QStringLiteral("Connected"), true},
                                           {QStringLiteral("Address"), QStringLiteral("AA:BB:CC:DD:EE:FF")}});
  bluetooth.setMuted(true);
  QTRY_VERIFY_WITH_TIMEOUT(bluetooth.errorMessage().contains(QStringLiteral("timed out")), 7000);
  // Retry after fixing the command must clear the mute operation's error,
  // independently of the intentionally missing test D-Bus service.
  bluetooth.setCommandRunnerForTest([](const QStringList &args, const auto &done) {
    done(args.first() == QStringLiteral("pw-dump")
        ? QByteArray(R"([{"id":42,"info":{"props":{"api.bluez5.address":"AA:BB:CC:DD:EE:FF"}}}])")
        : QByteArray(), QString());
  });
  bluetooth.setMuted(false);
  QVERIFY(!bluetooth.errorMessage().contains(QStringLiteral("timed out")));
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

  // A successful method reply alone must not dismiss the dialog.
  c.resolveTakeover(BluetoothClient::KeepCurrent);
  QCOMPARE(c.takeoverPending(), true);
  QCOMPARE(c.takeoverResolving(), true);
  QCOMPARE(c.takeoverIncomingName(), QStringLiteral("B"));
  c.bluezPropertyChangedForTest(dB, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Connected"), false}});
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.takeoverResolving(), false);
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

void TestControllers::testBluetoothTakeoverIncomingNameLiveUpdate() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  const QString dB = QStringLiteral("/org/bluez/hci0/dev_11_22_33_44_55_66");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  c.bluezObjectAddedForTest(dB, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Name"), QStringLiteral("Galaxy")},
                             {QStringLiteral("Connected"), QVariant(true)}});
  QCOMPARE(c.takeoverPending(), true);
  QCOMPARE(c.takeoverIncomingName(), QStringLiteral("Galaxy"));

  // Incoming device publishes its Alias while takeover is pending.
  c.bluezPropertyChangedForTest(dB, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Alias"), QStringLiteral("Bee")}});
  QCOMPARE(c.takeoverIncomingName(), QStringLiteral("Bee"));
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("A"));
}

void TestControllers::testBluetoothDeviceRemoveClearsPlayerState() {
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
                                                     QStringLiteral("Song")}})}});
  QCOMPARE(c.statusPublished(), true);

  // Drop the device (player object gone with it). AVRCP must go quiet and no
  // stale player registration may survive the removal.
  c.bluezObjectRemovedForTest(dA, QStringLiteral("org.bluez.MediaPlayer1"));
  c.bluezObjectRemovedForTest(dA, QStringLiteral("org.bluez.Device1"));
  QCOMPARE(c.statusPublished(), false);
  QCOMPARE(c.trackPublished(), false);

  // A fresh connection republishes cleanly; late props from the dead player
  // must not re-assert AVRCP for the default state.
  c.bluezPropertyChangedForTest(dA + QStringLiteral("/player0"),
                                QStringLiteral("org.bluez.MediaPlayer1"),
                                {{QStringLiteral("Status"), QStringLiteral("playing")}});
  QCOMPARE(c.statusPublished(), false);
  QCOMPARE(c.isBluetoothPlaying(), false);
  QCOMPARE(c.connectedDeviceName(), QString());
}

void TestControllers::testBluetoothEnsureDiscoverableCallsSet() {
  BluetoothClient c;
  bool setCalled = false;
  c.setDbusCallableForTest(
      [&setCalled](const QString &, const QString &objectPath,
                   const QString &interface, const QString &method,
                   const QVariantList &args,
                   const std::function<void(const QVariant &, const QString &)> &onFinished) {
        if (interface == QStringLiteral("org.freedesktop.DBus.Properties") &&
            method == QStringLiteral("Set")) {
          setCalled = true;
          QCOMPARE(objectPath, QStringLiteral("/org/bluez/hci0"));
          QCOMPARE(args.size(), 3);
          QCOMPARE(args.at(0).toString(), QStringLiteral("org.bluez.Adapter1"));
          QCOMPARE(args.at(1).toString(), QStringLiteral("Discoverable"));
          QCOMPARE(args.at(2).value<QDBusVariant>().variant().toBool(), true);
        }
        onFinished(QVariant(), QString());
      });
  c.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0"),
                            QStringLiteral("org.bluez.Adapter1"), QVariantMap());
  c.ensureDiscoverable();
  QVERIFY2(setCalled, "ensureDiscoverable() must call Properties.Set(Adapter1, Discoverable, true)");
}

void TestControllers::testBluetoothDiscoverabilityErrorAndRecovery() {
  BluetoothClient c;
  QCOMPARE(c.errorMessage(), QString());
  c.ensureDiscoverable();
  QCOMPARE(c.errorMessage(),
           QStringLiteral("Bluetooth adapter unavailable — check system config"));

  QString failure = QStringLiteral("org.freedesktop.DBus.Error.AccessDenied: denied");
  c.setDbusCallableForTest(
      [&failure](const QString &, const QString &, const QString &,
                 const QString &, const QVariantList &,
                 const std::function<void(const QVariant &, const QString &)> &finished) {
        finished(QVariant(), failure);
      });
  c.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0"),
                             QStringLiteral("org.bluez.Adapter1"), QVariantMap());
  c.ensureDiscoverable();
  QCOMPARE(c.errorMessage(),
           QStringLiteral("Permission denied — check system config"));
  failure = QStringLiteral("org.freedesktop.DBus.Error.NoReply: timed out");
  c.ensureDiscoverable();
  QCOMPARE(c.errorMessage(),
           QStringLiteral("Bluetooth discoverability: timed out — try again"));
  failure.clear();
  c.ensureDiscoverable();
  QCOMPARE(c.errorMessage(), QString());
}

void TestControllers::testBluetoothTransportErrorAndRecovery() {
  BluetoothClient c;
  QString failure = QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown");
  c.setDbusCallableForTest(
      [&failure](const QString &, const QString &, const QString &,
                  const QString &method, const QVariantList &,
                  const std::function<void(const QVariant &, const QString &)> &finished) {
        if (method == QStringLiteral("GetAll")) {
          finished(QVariantMap{{QStringLiteral("Status"), QStringLiteral("playing")}}, QString());
        } else {
          finished(QVariant(), failure);
        }
      });
  const QString path = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(path, QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Connected"), true}});
  c.bluezObjectAddedForTest(path + QStringLiteral("/player0"),
                             QStringLiteral("org.bluez.MediaPlayer1"),
                             {{QStringLiteral("Status"), QStringLiteral("playing")}});
  c.next();
  QCOMPARE(c.errorMessage(),
           QStringLiteral("Bluetooth Next: service unavailable — check system config"));
  failure.clear();
  c.next();
  QCOMPARE(c.errorMessage(), QString());
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
  QCOMPARE(c.takeoverPending(), true);
  QCOMPARE(c.takeoverResolving(), true);
  QCOMPARE(disconnected.size(), 1);
  QVERIFY(disconnected.contains(dB)); // the *new* device is kicked
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("A"));
  c.bluezPropertyChangedForTest(dB, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Connected"), false}});
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.takeoverResolving(), false);
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
  QCOMPARE(c.takeoverPending(), true);
  QCOMPARE(c.takeoverResolving(), true);
  QCOMPARE(disconnected.size(), 1);
  QVERIFY(disconnected.contains(dA)); // the *old* device is kicked
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("A"));
  c.bluezPropertyChangedForTest(dA, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Connected"), false}});
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.takeoverResolving(), false);
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("B"));
}

void TestControllers::testBluetoothTakeoverDisconnectFailureAndRetry() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_A");
  const QString dB = QStringLiteral("/org/bluez/hci0/dev_B");
  std::function<void(const QVariant &, const QString &)> pendingReply;
  int calls = 0;
  c.setDbusCallableForTest(
      [&pendingReply, &calls](const QString &, const QString &,
                              const QString &, const QString &method,
                              const QVariantList &,
                              const std::function<void(const QVariant &,
                                                       const QString &)> &done) {
        if (method == QStringLiteral("Disconnect")) {
          ++calls;
          pendingReply = done;
        }
      });
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Alias"), QStringLiteral("A")},
                              {QStringLiteral("Connected"), true}});
  c.bluezObjectAddedForTest(dB, QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Alias"), QStringLiteral("B")},
                              {QStringLiteral("Connected"), true}});

  c.resolveTakeover(BluetoothClient::KeepCurrent);
  QVERIFY(c.takeoverResolving());
  c.resolveTakeover(BluetoothClient::SwitchToNew); // no duplicate call
  QCOMPARE(calls, 1);
  auto lateReply = pendingReply;
  pendingReply(QVariant(), QStringLiteral("org.bluez.Error.NotAuthorized"));
  QVERIFY(c.takeoverPending());
  QVERIFY(!c.takeoverResolving());
  QCOMPARE(c.takeoverError(),
           QStringLiteral("Permission denied — check system config"));
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("A"));

  c.resolveTakeover(BluetoothClient::SwitchToNew);
  QCOMPARE(calls, 2);
  QCOMPARE(c.takeoverError(), QString());
  QVERIFY(c.takeoverResolving());
  lateReply(QVariant(), QStringLiteral("late failure"));
  QCOMPARE(c.takeoverError(), QString()); // stale callback must not affect retry
  pendingReply(QVariant(), QString());
  QVERIFY(c.takeoverResolving()); // method reply still awaits observed disconnect
  c.bluezPropertyChangedForTest(dA, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Connected"), false}});
  QVERIFY(!c.takeoverPending());
  QVERIFY(!c.takeoverResolving());
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("B"));
}

void TestControllers::testBluetoothTakeoverIncomingDisappearsDuringDisconnect() {
  BluetoothClient c;
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_A");
  const QString dB = QStringLiteral("/org/bluez/hci0/dev_B");
  std::function<void(const QVariant &, const QString &)> lateReply;
  c.setDbusCallableForTest(
      [&lateReply](const QString &, const QString &, const QString &,
                   const QString &method, const QVariantList &,
                   const std::function<void(const QVariant &,
                                            const QString &)> &done) {
        if (method == QStringLiteral("Disconnect")) {
          lateReply = done;
        }
      });
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Alias"), QStringLiteral("A")},
                              {QStringLiteral("Connected"), true}});
  c.bluezObjectAddedForTest(dB, QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Alias"), QStringLiteral("B")},
                              {QStringLiteral("Connected"), true}});
  c.resolveTakeover(BluetoothClient::KeepCurrent);
  c.bluezObjectRemovedForTest(dB, QStringLiteral("org.bluez.Device1"));
  QVERIFY(!c.takeoverPending());
  QVERIFY(!c.takeoverResolving());
  QCOMPARE(c.takeoverError(), QString());
  lateReply(QVariant(), QStringLiteral("org.bluez.Error.DoesNotExist"));
  QCOMPARE(c.takeoverError(), QString());
}

void TestControllers::testBluetoothTakeoverDisconnectTimeout() {
  BluetoothClient c;
  c.setDbusCallableForTest(
      [](const QString &, const QString &, const QString &,
         const QString &method, const QVariantList &,
         const std::function<void(const QVariant &, const QString &)> &done) {
        if (method == QStringLiteral("Disconnect")) {
          done(QVariant(), QString());
        }
      });
  c.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0/dev_A"),
                             QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Connected"), true}});
  c.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0/dev_B"),
                             QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Connected"), true}});
  c.resolveTakeover(BluetoothClient::KeepCurrent);
  QTRY_VERIFY_WITH_TIMEOUT(!c.takeoverResolving(), 6000);
  QVERIFY(c.takeoverPending());
  QVERIFY(c.takeoverError().contains(QStringLiteral("timed out")));
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
                        const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args.join(QStringLiteral(" ")));
        if (args.first() == QStringLiteral("pw-dump")) {
          onFinished(pwDump, QString());
        } else {
          onFinished(QByteArray(), QString());
        }
      });
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Address"), address},
                             {QStringLiteral("Connected"), QVariant(true)}});

  // Connecting re-asserts mute intent (here unmounted). Isolate the setMuted
  // under test from that connect-time call(s).
  calls.clear();
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
                        const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args.join(QStringLiteral(" ")));
        if (args.first() == QStringLiteral("pw-dump")) {
          onFinished(pwDump, QString());
        } else {
          onFinished(QByteArray(), QString());
        }
      });
  const QString dA = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
  c.bluezObjectAddedForTest(dA, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Address"), address},
                             {QStringLiteral("Connected"), QVariant(true)}});

  calls.clear(); // connect-time unmute is not under test here
  c.setMuted(false);
  QCOMPARE(c.muted(), false);
  QCOMPARE(calls.size(), 2);
  QCOMPARE(calls.last(), QStringLiteral("wpctl set-mute 35 0"));
}

void TestControllers::testBluetoothMuteCoversAllConnectedDevices() {
  BluetoothClient c;
  const QString addrA = QStringLiteral("AA:BB:CC:DD:EE:01");
  const QString addrB = QStringLiteral("AA:BB:CC:DD:EE:02");
  const QByteArray pwDump =
      "[{\"id\":35,\"type\":\"PipeWire:Interface:Node\",\"info\":{\"props\":"
      "{\"api.bluez5.address\":\"AA:BB:CC:DD:EE:01\"}}},{\"id\":41,\"type\":"
      "\"PipeWire:Interface:Node\",\"info\":{\"props\":{\"api.bluez5.address\":"
      "\"AA:BB:CC:DD:EE:02\"}}}]";
  c.setDbusCallableForTest(
      [](const QString &, const QString &, const QString &, const QString &,
         const QVariantList &,
         const std::function<void(const QVariant &, const QString &)> &onFinished) {
        onFinished(QVariant(), QString());
      });
  QStringList calls;
  c.setCommandRunnerForTest(
      [&calls, pwDump](const QStringList &args,
                        const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args.join(QStringLiteral(" ")));
        if (args.first() == QStringLiteral("pw-dump")) {
          onFinished(pwDump, QString());
        } else {
          onFinished(QByteArray(), QString());
        }
      });
  c.bluezObjectAddedForTest(
      QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_01"),
      QStringLiteral("org.bluez.Device1"),
      {{QStringLiteral("Alias"), QStringLiteral("A")},
       {QStringLiteral("Address"), addrA},
       {QStringLiteral("Connected"), QVariant(true)}});
  c.bluezObjectAddedForTest(
      QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_02"),
      QStringLiteral("org.bluez.Device1"),
      {{QStringLiteral("Alias"), QStringLiteral("B")},
       {QStringLiteral("Address"), addrB},
       {QStringLiteral("Connected"), QVariant(true)}});

  calls.clear(); // prior per-device connect-minute unmutes are not under test
  c.setMuted(true); // ADR 0008: mute EVERY connected A2DP node

  QStringList muteCmds;
  for (const QString &line : calls) {
    if (line.startsWith(QStringLiteral("wpctl"))) {
      muteCmds.append(line);
    }
  }
  QVERIFY(muteCmds.contains(QStringLiteral("wpctl set-mute 35 1")));
  QVERIFY(muteCmds.contains(QStringLiteral("wpctl set-mute 41 1")));
  QCOMPARE(muteCmds.size(), 2);
}

void TestControllers::testBluetoothMuteCommandFailure() {
  BluetoothClient c;
  const QString address = QStringLiteral("AA:BB:CC:DD:EE:FF");
  QString failure = QStringLiteral("Permission denied");
  c.setCommandRunnerForTest(
      [&failure](const QStringList &args,
                 const std::function<void(const QByteArray &, const QString &)> &finished) {
        if (args.first() == QStringLiteral("pw-dump")) {
          finished(QByteArrayLiteral(
              "[{\"id\":35,\"type\":\"PipeWire:Interface:Node\",\"info\":{\"props\":{"
              "\"api.bluez5.address\":\"AA:BB:CC:DD:EE:FF\"}}}]"), QString());
        } else {
          finished(QByteArray(), failure);
        }
      });
  c.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0/dev_A"),
                             QStringLiteral("org.bluez.Device1"),
                             {{QStringLiteral("Address"), address},
                              {QStringLiteral("Connected"), true}});
  c.setMuted(true);
  QCOMPARE(c.errorMessage(), QStringLiteral("Permission denied — check system config"));
  failure.clear();
  c.setMuted(true);
  QCOMPARE(c.errorMessage(), QString());
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

void TestControllers::testBluetoothRetargetsPlayerAfterTakeover() {
  BluetoothClient c;
  c.setDbusCallableForTest(
      [](const QString &, const QString &, const QString &,
         const QString &method, const QVariantList &,
         const std::function<void(const QVariant &, const QString &)> &done) {
        if (method == QStringLiteral("Disconnect")) {
          done(QVariant(), QString());
        } else if (method == QStringLiteral("GetAll")) {
          done(QVariant(), QStringLiteral("unavailable"));
        }
      });
  const QString first = QStringLiteral("/org/bluez/hci0/dev_A");
  const QString second = QStringLiteral("/org/bluez/hci0/dev_B");
  c.bluezObjectAddedForTest(first, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("A")},
                             {QStringLiteral("Connected"), true}});
  c.bluezObjectAddedForTest(first + QStringLiteral("/player0"),
                            QStringLiteral("org.bluez.MediaPlayer1"),
                            {{QStringLiteral("Track"), QVariantMap{
                                {QStringLiteral("Title"), QStringLiteral("First")}}}});
  c.bluezObjectAddedForTest(second, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Alias"), QStringLiteral("B")},
                             {QStringLiteral("Connected"), true}});
  c.bluezObjectAddedForTest(second + QStringLiteral("/player0"),
                            QStringLiteral("org.bluez.MediaPlayer1"),
                            {{QStringLiteral("Status"), QStringLiteral("playing")},
                             {QStringLiteral("Track"), QVariantMap{
                                 {QStringLiteral("Title"), QStringLiteral("Second")},
                                 {QStringLiteral("Duration"), 120000u}}}});
  QCOMPARE(c.trackTitle(), QStringLiteral("First"));
  c.resolveTakeover(BluetoothClient::SwitchToNew);
  c.bluezPropertyChangedForTest(first, QStringLiteral("org.bluez.Device1"),
                                {{QStringLiteral("Connected"), false}});
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("B"));
  QCOMPARE(c.trackTitle(), QStringLiteral("Second"));
  QCOMPARE(c.duration(), qint64(120000));
  QVERIFY(c.statusPublished());
  QVERIFY(c.isBluetoothPlaying());
}

void TestControllers::testBluetoothDeviceWithoutNameIsDetected() {
  BluetoothClient c;
  c.bluezObjectAddedForTest(
      QStringLiteral("/org/bluez/hci0/dev_A"),
      QStringLiteral("org.bluez.Device1"),
      {{QStringLiteral("Connected"), true}});
  QVERIFY(c.hasConnectedDevice());
  QCOMPARE(c.connectedDeviceName(), QStringLiteral("Bluetooth device"));
}

void TestControllers::testBluetoothPlayerAddedBeforeDevice() {
  BluetoothClient c;
  const QString device = QStringLiteral("/org/bluez/hci0/dev_A");
  c.bluezObjectAddedForTest(device + QStringLiteral("/player0"),
                            QStringLiteral("org.bluez.MediaPlayer1"),
                            {{QStringLiteral("Track"), QVariantMap{
                                {QStringLiteral("Title"), QStringLiteral("Early")}}}});
  c.bluezObjectAddedForTest(device, QStringLiteral("org.bluez.Device1"),
                            {{QStringLiteral("Connected"), true}});
  QCOMPARE(c.trackTitle(), QStringLiteral("Early"));
  QVERIFY(c.trackPublished());
}

void TestControllers::testBluetoothPrivateBusObjectManagerAndProperties() {
  QProcess daemon;
  daemon.start(QStringLiteral("dbus-daemon"),
               {QStringLiteral("--session"), QStringLiteral("--nofork"),
                QStringLiteral("--print-address=1")});
  QVERIFY(daemon.waitForStarted());
  QVERIFY(daemon.waitForReadyRead(5000));
  const QString address = QString::fromUtf8(daemon.readAllStandardOutput()).trimmed();
  QVERIFY(!address.isEmpty());
  const QString name = QStringLiteral("bluez-test-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces));
  QDBusConnection bus = QDBusConnection::connectToBus(address, name);
  QVERIFY(bus.isConnected());
  QVERIFY(bus.registerService(QStringLiteral("org.bluez")));
  MockBlueZObjects objects;
  QVERIFY(bus.registerVirtualObject(QStringLiteral("/"), &objects,
                                    QDBusConnection::SubPath));

  {
    BluetoothClient client(bus);
    QTRY_COMPARE_WITH_TIMEOUT(client.connectedDeviceName(),
                              QStringLiteral("Phone"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(client.trackTitle(),
                              QStringLiteral("Initial Track"), 3000);
    QCOMPARE(client.duration(), qint64(80000));
    QVERIFY(client.adapterPowered());

    const QString playerPath = QStringLiteral("/org/bluez/hci0/dev_A/player0");
    QDBusMessage changed = QDBusMessage::createSignal(
        playerPath, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    changed << QStringLiteral("org.bluez.MediaPlayer1")
            << QVariantMap{{QStringLiteral("Position"), 12000u},
                           {QStringLiteral("Track"),
                            QVariantMap{{QStringLiteral("Title"),
                                         QStringLiteral("Updated Track")},
                                        {QStringLiteral("Duration"), 95000u}}}}
            << QStringList();
    QVERIFY(bus.send(changed));
    QTRY_COMPARE_WITH_TIMEOUT(client.trackTitle(),
                              QStringLiteral("Updated Track"), 3000);
    QCOMPARE(client.duration(), qint64(95000));
    QCOMPARE(client.position(), qint64(12000));

    QDBusMessage invalidated = QDBusMessage::createSignal(
        playerPath, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    invalidated << QStringLiteral("org.bluez.MediaPlayer1") << QVariantMap()
                << QStringList{QStringLiteral("Track")};
    QVERIFY(bus.send(invalidated));
    QTRY_VERIFY_WITH_TIMEOUT(!client.trackPublished(), 3000);

    QDBusMessage disconnected = QDBusMessage::createSignal(
        QStringLiteral("/org/bluez/hci0/dev_A"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    disconnected << QStringLiteral("org.bluez.Device1")
                 << QVariantMap{{QStringLiteral("Connected"), false}}
                 << QStringList();
    QVERIFY(bus.send(disconnected));
    QTRY_VERIFY_WITH_TIMEOUT(!client.hasConnectedDevice(), 3000);

    QVERIFY(bus.unregisterService(QStringLiteral("org.bluez")));
    QTRY_VERIFY_WITH_TIMEOUT(!client.adapterPowered(), 3000);
    QVERIFY(bus.registerService(QStringLiteral("org.bluez")));
    QTRY_COMPARE_WITH_TIMEOUT(client.connectedDeviceName(),
                              QStringLiteral("Phone"), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(client.trackTitle(),
                              QStringLiteral("Initial Track"), 3000);
  }

  bus.unregisterObject(QStringLiteral("/"));
  bus.unregisterService(QStringLiteral("org.bluez"));
  QDBusConnection::disconnectFromBus(name);
  daemon.terminate();
  QVERIFY(daemon.waitForFinished(3000));
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

void TestControllers::testBackgroundDbusReadErrors_data() {
  QTest::addColumn<QString>("error");
  QTest::addColumn<QString>("expected");
  QTest::newRow("permission") << QStringLiteral("org.freedesktop.DBus.Error.AccessDenied")
                              << QStringLiteral("Permission denied — check system config");
  QTest::newRow("timeout") << QStringLiteral("org.freedesktop.DBus.Error.NoReply")
                           << QStringLiteral("timed out — try again");
  QTest::newRow("operation") << QStringLiteral("org.bluez.Error.Failed")
                             << QStringLiteral("failed:");
  QTest::newRow("invalid-reply") << QString() << QStringLiteral("failed:");
}

void TestControllers::testBackgroundDbusReadErrors() {
  QFETCH(QString, error);
  QFETCH(QString, expected);
  QProcess daemon;
  daemon.start(QStringLiteral("dbus-daemon"),
               {QStringLiteral("--session"), QStringLiteral("--nofork"),
                QStringLiteral("--print-address=1")});
  QVERIFY(daemon.waitForStarted());
  QVERIFY(daemon.waitForReadyRead(5000));
  const QString address = QString::fromUtf8(daemon.readAllStandardOutput()).trimmed();
  const QString name = QStringLiteral("read-error-test-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces));
  QDBusConnection bus = QDBusConnection::connectToBus(address, name);
  const QString bluezName = name + QStringLiteral("-bluez");
  QDBusConnection bluezBus = QDBusConnection::connectToBus(address, bluezName);
  const auto cleanup = qScopeGuard([&]() {
    QDBusConnection::disconnectFromBus(name);
    QDBusConnection::disconnectFromBus(bluezName);
    daemon.terminate();
    daemon.waitForFinished(3000);
  });
  QVERIFY(bus.isConnected());
  const QString service = QStringLiteral("org.mpris.MediaPlayer2.spotifyd.instance42");
  QVERIFY(bus.registerService(service));
  QVERIFY(bluezBus.registerService(QStringLiteral("org.bluez")));
  MockBlueZObjects objects;
  objects.lookupError = error;
  objects.invalidLookupReply = error.isEmpty();
  MockMprisPlayer player;
  player.readError = error;
  player.invalidReadReply = error.isEmpty();
  QVERIFY(bluezBus.registerVirtualObject(QStringLiteral("/"), &objects, QDBusConnection::SubPath));
  QVERIFY(bus.registerVirtualObject(QStringLiteral("/org/mpris/MediaPlayer2"), &player));

  SpotifyClient spotify(bus);
  BluetoothClient bluetooth(bus);
  QTRY_VERIFY_WITH_TIMEOUT(spotify.errorMessage().contains(expected), 3000);
  QTRY_VERIFY_WITH_TIMEOUT(bluetooth.errorMessage().contains(expected), 3000);
  QVERIFY(spotify.isAvailable());
  QVERIFY(!spotify.hasTrack());
  QVERIFY(!bluetooth.hasConnectedDevice());

  // An unrelated successful command must not hide a failed background read.
  spotify.next();
  QTRY_COMPARE_WITH_TIMEOUT(player.calls.size(), 1, 3000);
  QVERIFY(spotify.errorMessage().contains(expected));

  player.readError.clear();
  player.invalidReadReply = false;
  objects.lookupError.clear();
  objects.invalidLookupReply = false;
  QVERIFY(bus.unregisterService(service));
  QVERIFY(bluezBus.unregisterService(QStringLiteral("org.bluez")));
  QTRY_VERIFY_WITH_TIMEOUT(!spotify.isAvailable(), 3000);
  QCOMPARE(spotify.errorMessage(), QString()); // intentional shutdown is normal
  QVERIFY(bus.registerService(service));
  QVERIFY(bluezBus.registerService(QStringLiteral("org.bluez")));
  QTRY_VERIFY_WITH_TIMEOUT(spotify.hasTrack(), 3000);
  QTRY_VERIFY_WITH_TIMEOUT(bluetooth.hasConnectedDevice(), 3000);
  QTRY_COMPARE_WITH_TIMEOUT(spotify.errorMessage(), QString(), 3000);
  QTRY_COMPARE_WITH_TIMEOUT(bluetooth.errorMessage(), QString(), 3000);

  // AVRCP read failure is reported, then cleared by an observed player update.
  objects.playerError = QStringLiteral("org.freedesktop.DBus.Error.AccessDenied");
  const QString playerPath = QStringLiteral("/org/bluez/hci0/dev_A/player0");
  QDBusMessage added = QDBusMessage::createSignal(
      QStringLiteral("/"), QStringLiteral("org.freedesktop.DBus.ObjectManager"),
      QStringLiteral("InterfacesAdded"));
  added << QDBusObjectPath(playerPath)
        << QVariant::fromValue(QMap<QString, QVariantMap>{
             {QStringLiteral("org.bluez.MediaPlayer1"),
              {{QStringLiteral("Status"), QStringLiteral("playing")}}}});
  QVERIFY(bluezBus.send(added));
  QTRY_COMPARE_WITH_TIMEOUT(bluetooth.errorMessage(),
      QStringLiteral("Permission denied — check system config"), 3000);
  QDBusMessage changed = QDBusMessage::createSignal(
      playerPath, QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("PropertiesChanged"));
  changed << QStringLiteral("org.bluez.MediaPlayer1")
          << QVariantMap{{QStringLiteral("Position"), 15000u}} << QStringList();
  QVERIFY(bluezBus.send(changed));
  QTRY_COMPARE_WITH_TIMEOUT(bluetooth.position(), qint64(15000), 3000);
  QCOMPARE(bluetooth.errorMessage(), QString());
}

void TestControllers::testDisconnectedBusErrors() {
  const QDBusConnection bus(QStringLiteral("nonexistent-test-bus"));
  QVERIFY(!bus.isConnected());
  SpotifyClient spotify(bus);
  BluetoothClient bluetooth(bus);
  QTRY_VERIFY_WITH_TIMEOUT(!spotify.errorMessage().isEmpty(), 1000);
  QTRY_VERIFY_WITH_TIMEOUT(!bluetooth.errorMessage().isEmpty(), 1000);
  bluetooth.bluezObjectAddedForTest(QStringLiteral("/org/bluez/hci0"),
      QStringLiteral("org.bluez.Adapter1"), {{QStringLiteral("Powered"), true}});
  QVERIFY(!bluetooth.errorMessage().isEmpty());
}

void TestControllers::testSpotifyFirstArtistFromMetadata() {
  SpotifyClient client;
  QSignalSpy changed(&client, &SpotifyClient::artistChanged);
  client.setMetadataForTest(
      {{QStringLiteral("xesam:artist"), QStringList{QStringLiteral("AC/DC"),
                                                    QStringLiteral("Guest")}}});
  QCOMPARE(client.artist(), QStringLiteral("AC/DC, Guest"));
  QCOMPARE(client.firstArtist(), QStringLiteral("AC/DC"));
  QCOMPARE(changed.size(), 1);
}

void TestControllers::testSpotifyPrivateBusMprisLifecycle() {
  QProcess daemon;
  daemon.start(QStringLiteral("dbus-daemon"),
               {QStringLiteral("--session"), QStringLiteral("--nofork"),
                QStringLiteral("--print-address=1")});
  QVERIFY(daemon.waitForStarted());
  QVERIFY(daemon.waitForReadyRead(5000));
  const QString address = QString::fromUtf8(daemon.readAllStandardOutput()).trimmed();
  QVERIFY(!address.isEmpty());
  const QString connectionName = QStringLiteral("mpris-test-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces));
  QDBusConnection bus = QDBusConnection::connectToBus(address, connectionName);
  const auto cleanup = qScopeGuard([&]() {
    QDBusConnection::disconnectFromBus(connectionName);
    daemon.terminate();
    daemon.waitForFinished(3000);
  });
  QVERIFY(bus.isConnected());
  const QString service = QStringLiteral("org.mpris.MediaPlayer2.spotifyd.instance1234");
  QVERIFY(bus.registerService(service));
  MockMprisPlayer player;
  const QString path = QStringLiteral("/org/mpris/MediaPlayer2");
  QVERIFY(bus.registerVirtualObject(path, &player));

  {
    SpotifyClient client(bus);
    QSignalSpy available(&client, &SpotifyClient::availableChanged);
    QSignalSpy titleChanged(&client, &SpotifyClient::titleChanged);
    QTRY_VERIFY_WITH_TIMEOUT(client.isAvailable(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(client.title(), QStringLiteral("Initial Track"), 3000);
    QCOMPARE(client.artist(), QStringLiteral("First Artist, Guest"));
    QCOMPARE(client.firstArtist(), QStringLiteral("First Artist"));
    QCOMPARE(client.album(), QStringLiteral("First Album"));
    QCOMPARE(client.artUrl(), QStringLiteral("https://example.org/first.jpg"));
    QCOMPARE(client.duration(), qint64(190000));
    QCOMPARE(client.position(), qint64(5000));
    QVERIFY(client.hasTrack());
    QVERIFY(client.isSpotifyPlaying());
    QCOMPARE(available.size(), 1);
    QCOMPARE(titleChanged.size(), 1);

    client.pause();
    client.play();
    client.next();
    client.previous();
    client.seek(12000);
    QTRY_COMPARE_WITH_TIMEOUT(player.calls.size(), 5, 3000);
    QCOMPARE(player.calls.at(0).member(), QStringLiteral("Pause"));
    QCOMPARE(player.calls.at(1).member(), QStringLiteral("Play"));
    QCOMPARE(player.calls.at(2).member(), QStringLiteral("Next"));
    QCOMPARE(player.calls.at(3).member(), QStringLiteral("Previous"));
    QCOMPARE(player.calls.at(4).member(), QStringLiteral("SetPosition"));
    QCOMPARE(player.calls.at(4).arguments().at(0).value<QDBusObjectPath>().path(),
             QStringLiteral("/org/mpris/MediaPlayer2/Track/1"));
    QCOMPARE(player.calls.at(4).arguments().at(1).toLongLong(), qint64(12000000));

    player.denyCommands = true;
    client.next();
    QTRY_COMPARE_WITH_TIMEOUT(client.errorMessage(),
                              QStringLiteral("Permission denied — check system config"), 3000);
    player.denyCommands = false;
    client.next();
    QTRY_COMPARE_WITH_TIMEOUT(player.calls.size(), 7, 3000);
    QCOMPARE(client.errorMessage(), QString());

    QDBusMessage changed = QDBusMessage::createSignal(
        path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    changed << QStringLiteral("org.mpris.MediaPlayer2.Player")
            << QVariantMap{{QStringLiteral("PlaybackStatus"), QStringLiteral("Paused")},
                           {QStringLiteral("Position"), qint64(9000000)}}
            << QStringList();
    QVERIFY(bus.send(changed));
    QTRY_VERIFY_WITH_TIMEOUT(!client.isSpotifyPlaying(), 3000);
    QCOMPARE(client.position(), qint64(9000));

    player.metadata[QStringLiteral("xesam:title")] = QStringLiteral("Next Track");
    player.metadata[QStringLiteral("xesam:artist")] =
        QStringList{QStringLiteral("New Artist"), QStringLiteral("Guest")};
    QDBusMessage trackChanged = QDBusMessage::createSignal(
        path, QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    trackChanged << QStringLiteral("org.mpris.MediaPlayer2.Player")
                 << QVariantMap{{QStringLiteral("Metadata"), player.metadata}}
                 << QStringList();
    QVERIFY(bus.send(trackChanged));
    QTRY_COMPARE_WITH_TIMEOUT(client.title(), QStringLiteral("Next Track"), 3000);
    QCOMPARE(client.firstArtist(), QStringLiteral("New Artist"));
    QCOMPARE(titleChanged.size(), 2);

    QVERIFY(bus.unregisterService(service));
    QTRY_VERIFY_WITH_TIMEOUT(!client.hasTrack(), 3000);
    QVERIFY(!client.isAvailable());
    QVERIFY(bus.registerService(service));
    QTRY_VERIFY_WITH_TIMEOUT(client.hasTrack(), 3000);
    QVERIFY(client.isAvailable());

    QVERIFY(bus.unregisterService(service));
    QTRY_VERIFY_WITH_TIMEOUT(!client.isAvailable(), 3000);
    const QString daemonName = QStringLiteral("rs.spotifyd.instance1234");
    QVERIFY(bus.registerService(daemonName));
    QTRY_VERIFY_WITH_TIMEOUT(client.isAvailable(), 6000);
    QVERIFY(!client.hasTrack()); // spotifyd without a phone is a waiting state
    QVERIFY(bus.unregisterService(daemonName));
  }

  bus.unregisterObject(path);
  bus.unregisterService(service);
}

void TestControllers::testSpotifyIgnoresStaleStateReplies() {
  QProcess daemon;
  daemon.start(QStringLiteral("dbus-daemon"),
               {QStringLiteral("--session"), QStringLiteral("--nofork"),
                QStringLiteral("--print-address=1")});
  QVERIFY(daemon.waitForStarted());
  QVERIFY(daemon.waitForReadyRead(5000));
  const QString address = QString::fromUtf8(daemon.readAllStandardOutput()).trimmed();
  const QString name = QStringLiteral("stale-mpris-%1").arg(
      QUuid::createUuid().toString(QUuid::WithoutBraces));
  QDBusConnection bus = QDBusConnection::connectToBus(address, name);
  QDBusConnection server = QDBusConnection::connectToBus(address, name + QStringLiteral("-server"));
  const auto cleanup = qScopeGuard([&]() {
    QDBusConnection::disconnectFromBus(name);
    QDBusConnection::disconnectFromBus(name + QStringLiteral("-server"));
    daemon.terminate();
    daemon.waitForFinished(3000);
  });
  const QString service = QStringLiteral("org.mpris.MediaPlayer2.spotifyd.instance77");
  const QString path = QStringLiteral("/org/mpris/MediaPlayer2");
  MockMprisPlayer player;
  player.holdReads = true;
  QVERIFY(server.registerService(service));
  QVERIFY(server.registerVirtualObject(path, &player));
  SpotifyClient spotify(bus);
  QTRY_COMPARE_WITH_TIMEOUT(player.pendingReads.size(), 1, 3000);

  QDBusMessage changed = QDBusMessage::createSignal(
      path, QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("PropertiesChanged"));
  changed << QStringLiteral("org.mpris.MediaPlayer2.Player")
          << QVariantMap{{QStringLiteral("Metadata"), player.metadata}}
          << QStringList();
  QVERIFY(server.send(changed));
  QTRY_COMPARE_WITH_TIMEOUT(spotify.title(), QStringLiteral("Initial Track"), 3000);
  QSignalSpy errors(&spotify, &SpotifyClient::errorMessageChanged);
  QVERIFY(server.send(player.pendingReads.takeFirst().createErrorReply(
      QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"), QStringLiteral("stale failure"))));
  // A round-trip command ensures the earlier read reply has been processed.
  spotify.next();
  QTRY_COMPARE_WITH_TIMEOUT(player.calls.size(), 1, 3000);
  QCOMPARE(spotify.errorMessage(), QString());
  QCOMPARE(errors.size(), 0);

  // Start another delayed read, then lose the session before its reply arrives.
  QVERIFY(server.unregisterService(service));
  QTRY_VERIFY_WITH_TIMEOUT(!spotify.isAvailable(), 3000);
  QVERIFY(server.registerService(service));
  QTRY_COMPARE_WITH_TIMEOUT(player.pendingReads.size(), 1, 3000);
  QVERIFY(server.unregisterService(service));
  QTRY_VERIFY_WITH_TIMEOUT(!spotify.isAvailable(), 3000);
  QVERIFY(server.send(player.pendingReads.takeFirst().createReply(
      QVariantMap{{QStringLiteral("Metadata"), player.metadata}})));
  player.holdReads = false;
  QVERIFY(server.registerService(service));
  QTRY_VERIFY_WITH_TIMEOUT(spotify.hasTrack(), 3000);
  QCOMPARE(spotify.errorMessage(), QString());
  QCOMPARE(errors.size(), 0);
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
       [](const QStringList &, const std::function<void(const QByteArray &, const QString &)> &onFinished) {
         onFinished("Volume: 0.65\n", {});
      });
  c.pollNowForTest();
  QCOMPARE(c.volume(), 65);
}

void TestControllers::testVolumeControllerPollsExternalChanges() {
  VolumeController c;
  QByteArray current("Volume: 0.65\n");
  c.setCommandRunnerForTest(
       [&current](const QStringList &, const std::function<void(const QByteArray &, const QString &)> &onFinished) {
         onFinished(current, {});
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
                const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args);
         onFinished(QByteArray(), {});
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
                const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args);
         onFinished(QByteArray(), {});
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
  std::function<void(const QByteArray &, const QString &)> pendingReadFinish;
  QList<QStringList> calls;
  c.setCommandRunnerForTest(
      [&calls, &pendingReadFinish](const QStringList &args,
                                    const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args);
        if (args.first() == "get-volume") {
          pendingReadFinish = onFinished; // hold the read open (in flight)
        } else {
           onFinished(QByteArray(), {});
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
  pendingReadFinish("Volume: 0.50\n", {});
  QCOMPARE(c.volume(), 80);
}

void TestControllers::testVolumeCommandErrorsAndRecovery() {
  VolumeController c;
  QString writeError = QStringLiteral("Permission denied");
  QByteArray readOutput = "Volume: 0.50\n";
  int writes = 0;
  c.setCommandRunnerForTest(
      [&writeError, &readOutput, &writes](const QStringList &args,
                             const std::function<void(const QByteArray &, const QString &)> &done) {
        if (args.first() == QStringLiteral("set-volume")) {
          ++writes;
          done({}, writeError);
        } else {
          done(readOutput, readOutput.isEmpty() ? QStringLiteral("timed out") : QString());
        }
      });
  QSignalSpy errorSpy(&c, &VolumeController::errorMessageChanged);
  c.setVolume(75);
  QCOMPARE(c.errorMessage(), QStringLiteral("Permission denied — check system config"));
  c.pollNowForTest();
  QCOMPARE(c.errorMessage(), QStringLiteral("Permission denied — check system config"));
  QCOMPARE(c.volume(), 50);
  readOutput = "Volume: 0.75\n";
  c.pollNowForTest();
  QCOMPARE(c.errorMessage(), QString()); // observed recovery
  writeError = QStringLiteral("Permission denied");
  c.setVolume(65);
  c.setVolume(65); // the same value must allow manual retry
  QCOMPARE(writes, 3);
  writeError.clear();
  c.setVolume(65);
  QCOMPARE(c.errorMessage(), QString());
  readOutput.clear();
  c.pollNowForTest();
  QCOMPARE(c.errorMessage(), QStringLiteral("Volume read: timed out — try again"));
  QVERIFY(errorSpy.size() >= 3);
}

void TestControllers::testPowerCommandErrorsAndRetry() {
  PowerController c;
  std::function<void(const QString &)> pending;
  QStringList actions;
  c.setCommandRunnerForTest([&](const QString &action,
                                const std::function<void(const QString &)> &done) {
    actions.append(action);
    pending = done;
  });
  QSignalSpy successSpy(&c, &PowerController::commandSucceeded);
  c.shutdown();
  QVERIFY(c.busy());
  c.reboot(); // a second request cannot overtake the pending command
  QCOMPARE(actions, (QStringList{QStringLiteral("poweroff")}));
  pending(QStringLiteral("Failed to power off system via logind: Interactive authentication required."));
  QVERIFY(!c.busy());
  QCOMPARE(c.errorMessage(), QStringLiteral("Permission denied — check system config"));
  QCOMPARE(successSpy.size(), 0);
  c.reboot();
  QCOMPARE(actions.last(), QStringLiteral("reboot"));
  pending({});
  QCOMPARE(c.errorMessage(), QString());
  QCOMPARE(successSpy.size(), 1);
}

void TestControllers::testVolumeMuteRestoresLastLevel() {
  VolumeController c;
  QList<QStringList> calls;
  c.setCommandRunnerForTest(
      [&calls](const QStringList &args,
                const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args);
         onFinished(QByteArray(), {});
      });
  QSignalSpy volumeSpy(&c, &VolumeController::volumeChanged);

  c.setVolume(85);
  QVERIFY(!c.muted());
  c.setMuted(true);
  QCOMPARE(c.volume(), 0);
  QVERIFY(c.muted());
  QCOMPARE(calls.last(), (QStringList{"set-volume", "@DEFAULT_AUDIO_SINK@", "0%"}));
  c.setMuted(true);
  QCOMPARE(calls.size(), 2); // repeated mute must not overwrite the saved level

  c.setMuted(false);
  QCOMPARE(c.volume(), 85);
  QVERIFY(!c.muted());
  QCOMPARE(calls.last(), (QStringList{"set-volume", "@DEFAULT_AUDIO_SINK@", "85%"}));
  QCOMPARE(volumeSpy.size(), 3);
}

void TestControllers::testVolumeMuteFallsBackToTenPercent() {
  VolumeController c;
  QList<QStringList> calls;
  c.setCommandRunnerForTest(
      [&calls](const QStringList &args,
                const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args);
         onFinished(QByteArray(), {});
      });
  QVERIFY(c.muted());
  c.setMuted(false);
  QCOMPARE(c.volume(), 10);
  QCOMPARE(calls.last(), (QStringList{"set-volume", "@DEFAULT_AUDIO_SINK@", "10%"}));
}

void TestControllers::testVolumeMuteTracksSliderAndExternalChanges() {
  VolumeController c;
  QList<QStringList> calls;
  QByteArray current("Volume: 0.60\n");
  c.setCommandRunnerForTest(
      [&calls, &current](const QStringList &args,
                          const std::function<void(const QByteArray &, const QString &)> &onFinished) {
        calls.append(args);
         onFinished(args.first() == QStringLiteral("get-volume") ? current : QByteArray(), {});
      });
  c.pollNowForTest();
  QCOMPARE(c.volume(), 60);
  c.setMuted(true);
  QVERIFY(c.muted());

  // Moving the slider above zero is an unmute, and becomes the new restore level.
  c.setVolume(45);
  QVERIFY(!c.muted());
  c.setMuted(true);
  c.setMuted(false);
  QCOMPARE(c.volume(), 45);

  // The external knob's changes arrive via the existing 1-second poll.
  current = "Volume: 0.00\n";
  c.pollNowForTest();
  QVERIFY(c.muted());
  current = "Volume: 0.72\n";
  c.pollNowForTest();
  QCOMPARE(c.volume(), 72);
  QVERIFY(!c.muted());
  c.setMuted(true);
  c.setMuted(false);
  QCOMPARE(c.volume(), 72);
  QCOMPARE(calls.last(), (QStringList{"set-volume", "@DEFAULT_AUDIO_SINK@", "72%"}));
}

void TestControllers::testArtCacheDirCreation() {
  ArtCache cache;
  QVERIFY(!cache.cacheDir().isEmpty());
  QVERIFY(QDir(cache.cacheDir()).exists());
}

void TestControllers::testArtCacheDownloadReuseAndClear() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString oldPath = dir.filePath(QStringLiteral("old.jpg"));
  {
    QFile old(oldPath);
    QVERIFY(old.open(QIODevice::WriteOnly));
    QVERIFY(old.resize(100LL * 1024 * 1024));
  }
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  QImage image(2, 2, QImage::Format_RGB32);
  image.fill(Qt::red);
  QByteArray png;
  QBuffer buffer(&png);
  QVERIFY(buffer.open(QIODevice::WriteOnly));
  QVERIFY(image.save(&buffer, "PNG"));
  int requests = 0;
  connect(&server, &QTcpServer::newConnection, &server, [&]() {
    auto *socket = server.nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
      socket->readAll();
      ++requests;
      socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " +
                    QByteArray::number(png.size()) + "\r\nConnection: close\r\n\r\n" + png);
      socket->disconnectFromHost();
    });
  });
  QNetworkAccessManager manager;
  ArtCache cache(&manager, dir.path());
  QSignalSpy cached(&cache, &ArtCache::artCached);
  const QUrl url(QStringLiteral("http://127.0.0.1:%1/cover").arg(server.serverPort()));

  QVERIFY(cache.cacheArt(url, QStringLiteral("one")).isEmpty());
  QVERIFY(cache.cacheArt(url, QStringLiteral("two")).isEmpty());
  QTRY_COMPARE(cached.size(), 2);
  QCOMPARE(requests, 1);
  QCOMPARE(cached.at(0).at(0).toString(), QStringLiteral("one"));
  QCOMPARE(cached.at(1).at(0).toString(), QStringLiteral("two"));
  QVERIFY(!QFile::exists(oldPath)); // the new download pushed the cache over 100 MB
  const QUrl localUrl = cached.at(0).at(1).toUrl();
  QVERIFY(localUrl.isLocalFile());
  QFile file(localUrl.toLocalFile());
  QVERIFY(file.open(QIODevice::ReadOnly));
  QCOMPARE(file.readAll(), png);
  QCOMPARE(cache.cacheArt(url, QStringLiteral("again")), localUrl);
  QCOMPARE(requests, 1);
  cache.clearCache();
  QVERIFY(!QFile::exists(localUrl.toLocalFile()));
}

void TestControllers::testArtCachePrunesOldCovers() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString oldPath = dir.filePath(QStringLiteral("old.jpg"));
  const QString recentPath = dir.filePath(QStringLiteral("recent.png"));
  {
    QFile old(oldPath);
    QVERIFY(old.open(QIODevice::WriteOnly));
    QVERIFY(old.resize(75LL * 1024 * 1024));
    QFile recent(recentPath);
    QVERIFY(recent.open(QIODevice::WriteOnly));
    QVERIFY(recent.resize(40LL * 1024 * 1024));
    QVERIFY(old.setFileTime(QDateTime::currentDateTimeUtc().addDays(-2),
                            QFileDevice::FileModificationTime));
  }
  QNetworkAccessManager manager;
  ArtCache cache(&manager, dir.path());
  QVERIFY(!QFile::exists(oldPath));
  QVERIFY(QFile::exists(recentPath));
}

void TestControllers::testArtCacheRejectsFailuresAndInvalidImages() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  QTcpServer server;
  QVERIFY(server.listen(QHostAddress::LocalHost));
  int status = 404;
  connect(&server, &QTcpServer::newConnection, &server, [&]() {
    auto *socket = server.nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
      socket->readAll();
      const QByteArray result = status == 404 ? "HTTP/1.1 404 Not Found" : "HTTP/1.1 200 OK";
      socket->write(result + "\r\nContent-Length: 7\r\nConnection: close\r\n\r\ninvalid");
      socket->disconnectFromHost();
    });
  });
  QNetworkAccessManager manager;
  ArtCache cache(&manager, dir.path());
  QSignalSpy cached(&cache, &ArtCache::artCached);
  const QUrl url(QStringLiteral("http://127.0.0.1:%1/cover").arg(server.serverPort()));
  QVERIFY(cache.cacheArt(url, QStringLiteral("missing")).isEmpty());
  QTRY_COMPARE(cached.size(), 1);
  QVERIFY(cached.at(0).at(1).toUrl().isEmpty());
  status = 200;
  QVERIFY(cache.cacheArt(url, QStringLiteral("invalid")).isEmpty());
  QTRY_COMPARE(cached.size(), 2);
  QVERIFY(cached.at(1).at(1).toUrl().isEmpty());
  QCOMPARE(QDir(dir.path()).entryList({"*.jpg", "*.png"}, QDir::Files).size(), 0);
}

QTEST_MAIN(TestControllers)
#include "tst_controllers.moc"
