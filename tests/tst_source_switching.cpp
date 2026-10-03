#include "PlaybackController.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QProcess>
#include <QtTest>

class SourceServices : public QDBusVirtualObject {
public:
  QString unitState = QStringLiteral("active");
  bool powered = false;
  bool sink = true;
  bool holdStop = false;
  bool holdStart = false;
  bool holdPower = false;
  bool holdUnitReads = false;
  bool holdAdapterReads = false;
  bool malformedState = false;
  QString commandError;
  QStringList commands;
  QList<QDBusMessage> pendingReads;
  QList<QDBusMessage> pendingAdapterReads;
  const QString adapter = QStringLiteral("/org/bluez/hci0");
  const QString unit = QStringLiteral("/org/freedesktop/systemd1/unit/spotifyd_2eservice");

  QString introspect(const QString &) const override { return {}; }
  QVariantMap adapterProps() const {
    return {{QStringLiteral("Powered"), powered},
            {QStringLiteral("UUIDs"), sink
                ? QStringList{QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb")}
                : QStringList{}}};
  }
  bool handleMessage(const QDBusMessage &message, const QDBusConnection &bus) override {
    const auto reject = [&]() {
      bus.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.InvalidArgs"),
                                        QStringLiteral("Incorrect path, interface, or signature")));
      return true;
    };
    if (message.interface() == QStringLiteral("org.freedesktop.systemd1.Manager")) {
      if (message.path() != QStringLiteral("/org/freedesktop/systemd1") ||
          message.arguments().isEmpty() ||
          message.arguments().first().toString() != QStringLiteral("spotifyd.service")) {
        return reject();
      }
      if (message.member() == QStringLiteral("LoadUnit") && message.signature() == "s") {
        bus.send(message.createReply(QVariant::fromValue(QDBusObjectPath(unit))));
        return true;
      }
      if ((message.member() == QStringLiteral("StartUnit") || message.member() == QStringLiteral("StopUnit")) &&
          message.signature() == "ss" && message.arguments().at(1).toString() == QStringLiteral("replace")) {
        const bool start = message.member() == QStringLiteral("StartUnit");
        commands.append(start ? QStringLiteral("startSpotify") : QStringLiteral("stopSpotify"));
        if (!commandError.isEmpty()) {
          bus.send(message.createErrorReply(commandError, QStringLiteral("Mock source operation failed")));
        } else {
          if (start && !holdStart) {
            unitState = QStringLiteral("active");
          } else if (!start && !holdStop) {
            unitState = QStringLiteral("inactive");
          }
          bus.send(message.createReply(QVariant::fromValue(QDBusObjectPath(
              QStringLiteral("/org/freedesktop/systemd1/job/1")))));
        }
        return true;
      }
      return reject();
    }
    if (message.interface() == QStringLiteral("org.freedesktop.DBus.ObjectManager") &&
        message.member() == QStringLiteral("GetManagedObjects") && message.path() == "/") {
      QDBusArgument arg;
      arg.beginMap(QMetaType::fromType<QDBusObjectPath>(),
                   QMetaType::fromType<QMap<QString, QVariantMap>>());
      arg.beginMapEntry();
      arg << QDBusObjectPath(adapter)
          << QMap<QString, QVariantMap>{{QStringLiteral("org.bluez.Adapter1"), adapterProps()}};
      arg.endMapEntry();
      arg.endMap();
      bus.send(message.createReply(QVariant::fromValue(arg)));
      return true;
    }
    if (message.interface() == QStringLiteral("org.freedesktop.DBus.Properties")) {
      if (message.member() == QStringLiteral("GetAll") && message.signature() == "s") {
        if (message.path() == adapter && message.arguments().first().toString() == QStringLiteral("org.bluez.Adapter1")) {
          if (holdAdapterReads) {
            pendingAdapterReads.append(message);
          } else {
            bus.send(message.createReply(adapterProps()));
          }
        } else if (message.path() == unit && message.arguments().first().toString() == QStringLiteral("org.freedesktop.systemd1.Unit")) {
          if (holdUnitReads) {
            pendingReads.append(message);
          } else if (malformedState) {
            bus.send(message.createReply(QStringLiteral("bad state")));
          } else {
            bus.send(message.createReply(QVariantMap{{QStringLiteral("ActiveState"), unitState}}));
          }
        } else {
          return reject();
        }
        return true;
      }
      if (message.member() == QStringLiteral("Set") && message.signature() == "ssv" &&
          message.path() == adapter && message.arguments().first().toString() == QStringLiteral("org.bluez.Adapter1")) {
        const QString property = message.arguments().at(1).toString();
        const QVariant value = message.arguments().at(2).value<QDBusVariant>().variant();
        if (value.metaType() != QMetaType::fromType<bool>()) {
          return reject();
        }
        if (property == QStringLiteral("Powered")) {
          commands.append(value.toBool() ? QStringLiteral("powerOn") : QStringLiteral("powerOff"));
          if (!commandError.isEmpty()) {
            bus.send(message.createErrorReply(commandError, QStringLiteral("Mock adapter operation failed")));
            return true;
          }
          if (!holdPower) {
            powered = value.toBool();
          }
        } else if (property != QStringLiteral("Discoverable")) {
          return reject();
        }
        bus.send(message.createReply());
        return true;
      }
      return reject();
    }
    if (message.interface() == QStringLiteral("org.bluez.AgentManager1")) {
      bus.send(message.createReply());
      return true;
    }
    return false;
  }
};

class TestSourceSwitching : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void bootAndOrderedTransitions();
  void shutdownMustBeObserved();
  void permissionFailureAndManualRetry();
  void timeoutAndLateRecovery();
  void sinkAndServiceReadiness();
  void staleReadsCannotAdvance();
  void malformedStateIsAnError();
  void outgoingTimeoutRequiresRetry();
  void failedSpotifyStartupAndRecovery();
  void staleAdapterReadAfterLiveUpdate();

private:
  QProcess daemon;
  QDBusConnection client = QDBusConnection(QStringLiteral("source-client"));
  QDBusConnection server = QDBusConnection(QStringLiteral("source-server"));
  SourceServices services;
  void boot(PlaybackController &playback);
};

void TestSourceSwitching::initTestCase() {
  qDBusRegisterMetaType<QMap<QString, QVariantMap>>();
  daemon.start(QStringLiteral("dbus-daemon"),
      {QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
  QVERIFY(daemon.waitForStarted());
  QVERIFY(daemon.waitForReadyRead());
  const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
  client = QDBusConnection::connectToBus(address, QStringLiteral("source-client"));
  server = QDBusConnection::connectToBus(address, QStringLiteral("source-server"));
  QVERIFY(server.registerService(QStringLiteral("org.bluez")));
  QVERIFY(server.registerService(QStringLiteral("org.freedesktop.systemd1")));
  QVERIFY(server.registerVirtualObject(QStringLiteral("/"), &services, QDBusConnection::SubPath));
}

void TestSourceSwitching::cleanupTestCase() {
  QDBusConnection::disconnectFromBus(QStringLiteral("source-client"));
  QDBusConnection::disconnectFromBus(QStringLiteral("source-server"));
  daemon.terminate();
  QVERIFY(daemon.waitForFinished());
}

void TestSourceSwitching::init() {
  services.unitState = QStringLiteral("active");
  services.powered = false;
  services.sink = true;
  services.holdStop = false;
  services.holdStart = false;
  services.holdPower = false;
  services.holdUnitReads = false;
  services.holdAdapterReads = false;
  services.malformedState = false;
  services.commandError.clear();
  services.commands.clear();
  services.pendingReads.clear();
  services.pendingAdapterReads.clear();
}

void TestSourceSwitching::boot(PlaybackController &playback) {
  QTRY_VERIFY(playback.sourceReady());
  QVERIFY(!playback.switching());
  QCOMPARE(playback.playbackState(), PlaybackController::SpotifyWaiting);
  QVERIFY(playback.sourceError().isEmpty()); // no MPRIS or phone is expected
}

void TestSourceSwitching::bootAndOrderedTransitions() {
  PlaybackController playback(client, client);
  auto *deadline = playback.findChild<QTimer *>(QStringLiteral("sourceTransitionDeadline"));
  QVERIFY(deadline);
  QCOMPARE(deadline->interval(), 10000);
  boot(playback);
  QVERIFY(services.commands.isEmpty());
  playback.switchToBluetooth();
  QCOMPARE(playback.playbackState(), PlaybackController::BluetoothWaiting);
  QTRY_VERIFY(playback.sourceReady());
  QCOMPARE(services.commands, QStringList({QStringLiteral("stopSpotify"), QStringLiteral("powerOn")}));
  playback.switchToSpotify();
  QCOMPARE(playback.playbackState(), PlaybackController::SpotifyWaiting);
  QTRY_VERIFY(playback.sourceReady());
  QCOMPARE(services.commands, QStringList({QStringLiteral("stopSpotify"), QStringLiteral("powerOn"),
      QStringLiteral("powerOff"), QStringLiteral("startSpotify")}));
}

void TestSourceSwitching::shutdownMustBeObserved() {
  PlaybackController playback(client, client);
  boot(playback);
  services.holdStop = true;
  playback.switchToBluetooth();
  QTRY_COMPARE(services.commands.size(), 1);
  QTest::qWait(650);
  QVERIFY(playback.switching());
  QVERIFY(!services.powered); // successful StopUnit reply is insufficient
  services.unitState = QStringLiteral("inactive");
  QTRY_VERIFY(playback.sourceReady());
  QCOMPARE(services.commands.last(), QStringLiteral("powerOn"));

  services.holdPower = true;
  playback.switchToSpotify();
  QTRY_COMPARE(services.commands.last(), QStringLiteral("powerOff"));
  QTest::qWait(650);
  QVERIFY(playback.switching());
  QCOMPARE(services.unitState, QStringLiteral("inactive"));
  services.powered = false;
  QTRY_VERIFY(playback.sourceReady());
  QCOMPARE(services.commands.last(), QStringLiteral("startSpotify"));
}

void TestSourceSwitching::permissionFailureAndManualRetry() {
  PlaybackController playback(client, client);
  boot(playback);
  services.commandError = QStringLiteral("org.freedesktop.DBus.Error.AccessDenied");
  playback.switchToBluetooth();
  QTRY_VERIFY(!playback.switching());
  QVERIFY(playback.sourceError().contains(QStringLiteral("Permission denied")));
  QCOMPARE(playback.playbackState(), PlaybackController::BluetoothWaiting);
  QVERIFY(!services.powered);
  QTest::qWait(650);
  QCOMPARE(services.commands.size(), 1); // passive observations never retry
  services.commandError.clear();
  playback.retrySource();
  QTRY_VERIFY(playback.sourceReady());
  QVERIFY(playback.sourceError().isEmpty());
  QCOMPARE(services.commands, QStringList({QStringLiteral("stopSpotify"),
      QStringLiteral("stopSpotify"), QStringLiteral("powerOn")}));

  services.commandError = QStringLiteral("org.bluez.Error.NotAuthorized");
  playback.switchToSpotify();
  QTRY_VERIFY(!playback.switching());
  QCOMPARE(playback.playbackState(), PlaybackController::SpotifyWaiting);
  QVERIFY(playback.sourceError().contains(QStringLiteral("Permission denied")));
  QVERIFY(!services.commands.contains(QStringLiteral("startSpotify")));
}

void TestSourceSwitching::timeoutAndLateRecovery() {
  PlaybackController playback(client, client, nullptr, 250);
  boot(playback);
  services.holdPower = true;
  playback.switchToBluetooth();
  QTRY_VERIFY(!playback.switching());
  QVERIFY(playback.sourceError().contains(QStringLiteral("timed out")));
  QCOMPARE(playback.playbackState(), PlaybackController::BluetoothWaiting);
  QCOMPARE(services.commands, QStringList({QStringLiteral("stopSpotify"), QStringLiteral("powerOn")}));
  QTest::qWait(650);
  QCOMPARE(services.commands.size(), 2);
  services.powered = true; // delayed observed startup, without another command
  QTRY_VERIFY(playback.sourceReady());
  QVERIFY(playback.sourceError().isEmpty());
  QCOMPARE(services.commands.size(), 2);
}

void TestSourceSwitching::sinkAndServiceReadiness() {
  PlaybackController playback(client, client);
  boot(playback);
  services.sink = false;
  playback.switchToBluetooth();
  QTRY_VERIFY(services.powered);
  QTest::qWait(650);
  QVERIFY(playback.switching());
  QVERIFY(!playback.sourceReady());
  services.sink = true;
  QTRY_VERIFY(playback.sourceReady());
  services.holdStart = true;
  playback.switchToSpotify();
  QTRY_COMPARE(services.commands.last(), QStringLiteral("startSpotify"));
  services.unitState = QStringLiteral("activating");
  QTest::qWait(650);
  QVERIFY(playback.switching());
  services.unitState = QStringLiteral("active");
  QTRY_VERIFY(playback.sourceReady());
  QCOMPARE(playback.playbackState(), PlaybackController::SpotifyWaiting);
}

void TestSourceSwitching::staleReadsCannotAdvance() {
  PlaybackController playback(client, client);
  boot(playback);
  services.holdUnitReads = true;
  QTRY_VERIFY(!services.pendingReads.isEmpty());
  playback.switchToBluetooth();
  QTRY_VERIFY(services.pendingReads.size() >= 2);
  const auto stale = services.pendingReads.takeFirst();
  server.send(stale.createReply(QVariantMap{{QStringLiteral("ActiveState"), QStringLiteral("inactive")}}));
  QTest::qWait(100);
  QVERIFY(!services.powered);
  services.holdUnitReads = false;
  for (const auto &message : std::as_const(services.pendingReads)) {
    server.send(message.createReply(QVariantMap{{QStringLiteral("ActiveState"), QStringLiteral("inactive")}}));
  }
  services.pendingReads.clear();
  QTRY_VERIFY(playback.sourceReady());
}

void TestSourceSwitching::malformedStateIsAnError() {
  services.malformedState = true;
  PlaybackController playback(client, client);
  QTRY_VERIFY(!playback.sourceError().isEmpty());
  QVERIFY(!playback.sourceReady());
  QVERIFY(services.commands.isEmpty());
  services.malformedState = false;
  QTRY_VERIFY(playback.sourceReady());
  QVERIFY(playback.sourceError().isEmpty());
}

void TestSourceSwitching::outgoingTimeoutRequiresRetry() {
  PlaybackController playback(client, client, nullptr, 250);
  boot(playback);
  services.holdStop = true;
  playback.switchToBluetooth();
  QTRY_VERIFY(!playback.switching());
  QVERIFY(playback.sourceError().contains(QStringLiteral("timed out")));
  services.unitState = QStringLiteral("inactive");
  QTest::qWait(650);
  QVERIFY(!services.powered);
  QCOMPARE(services.commands, QStringList{QStringLiteral("stopSpotify")});
  // Timeout freezes command progression; only the user's retry can issue the
  // never-started incoming source after a delayed shutdown.
  playback.retrySource();
  QTRY_VERIFY(playback.sourceReady());
  QCOMPARE(services.commands.last(), QStringLiteral("powerOn"));
}

void TestSourceSwitching::failedSpotifyStartupAndRecovery() {
  PlaybackController playback(client, client);
  boot(playback);
  playback.switchToBluetooth();
  QTRY_VERIFY(playback.sourceReady());
  services.holdStart = true;
  playback.switchToSpotify();
  QTRY_COMPARE(services.commands.last(), QStringLiteral("startSpotify"));
  services.unitState = QStringLiteral("failed");
  QTRY_VERIFY(!playback.switching());
  QVERIFY(playback.sourceError().contains(QStringLiteral("startup failed")));
  const int before = services.commands.size();
  QTest::qWait(650);
  QCOMPARE(services.commands.size(), before);
  services.unitState = QStringLiteral("active");
  QTRY_VERIFY(playback.sourceReady());
  QVERIFY(playback.sourceError().isEmpty());
  QCOMPARE(services.commands.size(), before);
  QCOMPARE(playback.playbackState(), PlaybackController::SpotifyWaiting);
}

void TestSourceSwitching::staleAdapterReadAfterLiveUpdate() {
  PlaybackController playback(client, client);
  boot(playback);
  services.holdAdapterReads = true;
  QTRY_VERIFY(!services.pendingAdapterReads.isEmpty());
  playback.switchToBluetooth();
  QTRY_VERIFY(services.pendingAdapterReads.size() >= 2);
  const auto oldRead = services.pendingAdapterReads.takeFirst();
  server.send(oldRead.createReply(QVariantMap{{QStringLiteral("Powered"), false},
      {QStringLiteral("UUIDs"), QStringList{}}}));
  QTest::qWait(100);
  QVERIFY(!playback.sourceReady()); // the pre-command read must be ignored
  auto signal = QDBusMessage::createSignal(services.adapter,
      QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
  signal << QStringLiteral("org.bluez.Adapter1") << services.adapterProps() << QStringList{};
  server.send(signal);
  QTRY_VERIFY(playback.sourceReady());
  for (const auto &read : std::as_const(services.pendingAdapterReads)) {
    server.send(read.createReply(QVariantMap{{QStringLiteral("Powered"), false},
        {QStringLiteral("UUIDs"), QStringList{}}}));
  }
  services.pendingAdapterReads.clear();
  QTest::qWait(100);
  QVERIFY(playback.sourceReady()); // a live update supersedes the held read
  QVERIFY(playback.sourceError().isEmpty());
}

QTEST_GUILESS_MAIN(TestSourceSwitching)
#include "tst_source_switching.moc"
