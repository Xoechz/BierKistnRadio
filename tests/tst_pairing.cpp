#include "PairingAgent.h"
#include "BluetoothClient.h"

#include <QDBusPendingCallWatcher>
#include <QDBusVirtualObject>
#include <QProcess>
#include <QtTest>

class MockAgentManager : public QDBusVirtualObject {
public:
  QString path;
  QString capability;
  int defaults = 0;
  int unregisters = 0;
  QStringList canceledDevices;
  bool denyDefault = false;
  QString introspect(const QString &) const override { return {}; }
  bool handleMessage(const QDBusMessage &message,
                     const QDBusConnection &bus) override {
    if (message.interface() == QStringLiteral("org.bluez.Device1") &&
        message.member() == QStringLiteral("CancelPairing") && message.signature().isEmpty()) {
      canceledDevices.append(message.path());
      bus.send(message.createReply());
      return true;
    }
    if (message.interface() != QStringLiteral("org.bluez.AgentManager1")) {
      return false;
    }
    if (message.member() == QStringLiteral("RegisterAgent") && message.signature() == "os") {
      path = message.arguments().at(0).value<QDBusObjectPath>().path();
      capability = message.arguments().at(1).toString();
    } else if (message.member() == QStringLiteral("RequestDefaultAgent") && message.signature() == "o") {
      ++defaults;
      if (denyDefault) {
        bus.send(message.createErrorReply(QStringLiteral("org.bluez.Error.NotAuthorized"),
                                          QStringLiteral("Permission denied")));
        return true;
      }
    } else if (message.member() == QStringLiteral("UnregisterAgent") && message.signature() == "o") {
      ++unregisters;
    } else {
      bus.send(message.createErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid manager call")));
      return true;
    }
    bus.send(message.createReply());
    return true;
  }
};

class TestPairing : public QObject {
  Q_OBJECT
private slots:
  void initTestCase();
  void cleanupTestCase();
  void confirmationAndStaleDecisions();
  void displayUpdatesCancelAndExit();
  void timeoutAndServiceAuthorization();
  void registrationErrorAndRecovery();
  void senderValidationAndDaemonRestart();
  void adapterAvailabilityAndPairedReconnect();

private:
  QProcess daemon;
  QString address;
  QDBusConnection app = QDBusConnection(QStringLiteral("pairing-app"));
  QDBusConnection bluez = QDBusConnection(QStringLiteral("pairing-bluez"));
  MockAgentManager manager;
  QDBusPendingCall call(const QString &method, const QVariantList &args = {});
  void ready(PairingAgent &agent);
};

void TestPairing::initTestCase() {
  daemon.start(QStringLiteral("dbus-daemon"),
               {QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
  QVERIFY(daemon.waitForStarted());
  QVERIFY(daemon.waitForReadyRead());
  address = QString::fromUtf8(daemon.readLine()).trimmed();
  app = QDBusConnection::connectToBus(address, QStringLiteral("pairing-app"));
  bluez = QDBusConnection::connectToBus(address, QStringLiteral("pairing-bluez"));
  QVERIFY(app.isConnected());
  QVERIFY(bluez.registerService(QStringLiteral("org.bluez")));
  QVERIFY(bluez.registerVirtualObject(QStringLiteral("/org/bluez"), &manager,
                                      QDBusConnection::SubPath));
}

void TestPairing::cleanupTestCase() {
  QDBusConnection::disconnectFromBus(QStringLiteral("pairing-app"));
  QDBusConnection::disconnectFromBus(QStringLiteral("pairing-bluez"));
  daemon.terminate();
  QVERIFY(daemon.waitForFinished());
}

QDBusPendingCall TestPairing::call(const QString &method, const QVariantList &args) {
  auto message = QDBusMessage::createMethodCall(app.baseService(),
      QStringLiteral("/org/bierkistn/PairingAgent"), QStringLiteral("org.bluez.Agent1"), method);
  message.setArguments(args);
  return bluez.asyncCall(message, 40000);
}

void TestPairing::ready(PairingAgent &agent) {
  const int before = manager.defaults;
  agent.setDeviceLookup([](const QString &) { return QStringLiteral("Test Phone"); },
                        [](const QString &path) { return path.endsWith(QStringLiteral("paired")); });
  agent.setEnabled(true);
  QTRY_COMPARE(manager.defaults, before + 1);
  QCOMPARE(manager.capability, QStringLiteral("DisplayYesNo"));
  QCOMPARE(manager.path, QStringLiteral("/org/bierkistn/PairingAgent"));
}

void TestPairing::confirmationAndStaleDecisions() {
  PairingAgent agent(app);
  ready(agent);
  const QVariant device = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_AA")));
  QDBusPendingCallWatcher first(call(QStringLiteral("RequestConfirmation"), {device, uint(42)}));
  QTRY_VERIFY(agent.pending());
  QCOMPARE(agent.passkey(), QStringLiteral("000042"));
  QCOMPARE(agent.deviceName(), QStringLiteral("Test Phone"));
  QVERIFY(!first.isFinished()); // no acknowledgement before touchscreen consent
  const quint64 firstId = agent.requestId();

  QDBusPendingCallWatcher duplicate(call(QStringLiteral("RequestConfirmation"), {device, uint(42)}));
  QTRY_VERIFY(duplicate.isFinished());
  QCOMPARE(duplicate.error().name(), QStringLiteral("org.bluez.Error.Rejected"));
  QVERIFY(agent.pending());
  agent.resolve(firstId, true);
  QTRY_VERIFY(first.isFinished());
  QVERIFY(!first.isError());
  QVERIFY(!agent.pending());

  QDBusPendingCallWatcher second(call(QStringLiteral("RequestConfirmation"), {device, uint(123456)}));
  QTRY_VERIFY(agent.pending());
  agent.resolve(firstId, true); // old dialog must not accept the new pairing
  QVERIFY(agent.pending());
  QVERIFY(!second.isFinished());
  agent.resolve(agent.requestId(), false);
  QTRY_VERIFY(second.isFinished());
  QCOMPARE(second.error().name(), QStringLiteral("org.bluez.Error.Rejected"));
}

void TestPairing::displayUpdatesCancelAndExit() {
  auto agent = std::make_unique<PairingAgent>(app);
  ready(*agent);
  const QVariant device = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_BB")));
  QSignalSpy errors(agent.get(), &PairingAgent::errorChanged);
  const int canceledBefore = manager.canceledDevices.size();
  QDBusPendingCallWatcher first(call(QStringLiteral("DisplayPasskey"), {device, uint(7), QVariant::fromValue(ushort(0))}));
  QTRY_VERIFY(first.isFinished());
  QTRY_COMPARE(manager.canceledDevices.size(), canceledBefore + 1);
  QVERIFY(!agent->pending()); // notification replies are not a consent gate
  QVERIFY(!errors.isEmpty());
  QVERIFY(errors.last().first().toString().contains(QStringLiteral("matching-code")));
  QDBusPendingCallWatcher update(call(QStringLiteral("DisplayPasskey"), {device, uint(7), QVariant::fromValue(ushort(3))}));
  QTRY_VERIFY(update.isFinished());
  QTRY_COMPARE(manager.canceledDevices.size(), canceledBefore + 2);
  QDBusPendingCallWatcher confirmation(call(QStringLiteral("RequestConfirmation"), {device, uint(7)}));
  QTRY_VERIFY(agent->pending());
  QDBusPendingCallWatcher cancel(call(QStringLiteral("Cancel")));
  QTRY_VERIFY(cancel.isFinished());
  QTRY_VERIFY(confirmation.isFinished());
  QVERIFY(confirmation.isError());
  QVERIFY(!agent->pending());

  QDBusPendingCallWatcher pending(call(QStringLiteral("RequestConfirmation"), {device, uint(765432)}));
  QTRY_VERIFY(agent->pending());
  agent.reset();
  QTRY_VERIFY(pending.isFinished());
  QCOMPARE(pending.error().name(), QStringLiteral("org.bluez.Error.Rejected"));
}

void TestPairing::timeoutAndServiceAuthorization() {
  PairingAgent agent(app);
  ready(agent);
  const QVariant device = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_CC")));
  QDBusPendingCallWatcher pending(call(QStringLiteral("RequestConfirmation"), {device, uint(111111)}));
  QTRY_VERIFY(agent.pending());
  QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 35000);
  QCOMPARE(pending.error().name(), QStringLiteral("org.bluez.Error.Rejected"));
  QVERIFY(!agent.pending());
  agent.resolve(agent.requestId(), true); // timed-out requests cannot resurrect
  QDBusPendingCallWatcher unknown(call(QStringLiteral("AuthorizeService"), {device, QStringLiteral("audio")}));
  QTRY_VERIFY(unknown.isFinished());
  QVERIFY(unknown.isError());
  const QVariant paired = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_paired")));
  QDBusPendingCallWatcher reconnect(call(QStringLiteral("AuthorizeService"), {paired, QStringLiteral("audio")}));
  QTRY_VERIFY(reconnect.isFinished());
  QVERIFY(!reconnect.isError());
  QVERIFY(!agent.pending());
  QDBusPendingCallWatcher justWorks(call(QStringLiteral("RequestAuthorization"), {device}));
  QTRY_VERIFY(justWorks.isFinished());
  QVERIFY(justWorks.isError());
}

void TestPairing::registrationErrorAndRecovery() {
  PairingAgent agent(app);
  QSignalSpy errors(&agent, &PairingAgent::errorChanged);
  manager.denyDefault = true;
  ready(agent);
  QTRY_VERIFY(!errors.isEmpty());
  QVERIFY(errors.last().first().toString().contains(QStringLiteral("Permission denied")));
  agent.setEnabled(false);
  manager.denyDefault = false;
  const int count = errors.count();
  ready(agent);
  QTRY_VERIFY(errors.count() > count);
  QCOMPARE(errors.last().first().toString(), QString());
  const QVariant device = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_DD")));
  QDBusPendingCallWatcher pending(call(QStringLiteral("RequestConfirmation"), {device, uint(222222)}));
  QTRY_VERIFY(agent.pending());
  agent.setEnabled(false);
  QTRY_VERIFY(pending.isFinished());
  QVERIFY(pending.isError());
  QDBusPendingCallWatcher disabled(call(QStringLiteral("RequestConfirmation"), {device, uint(222222)}));
  QTRY_VERIFY(disabled.isFinished());
  QVERIFY(disabled.isError());
}

void TestPairing::senderValidationAndDaemonRestart() {
  PairingAgent agent(app);
  ready(agent);
  const QVariant device = QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/bluez/hci0/dev_EE")));
  QDBusConnection outsider = QDBusConnection::connectToBus(address, QStringLiteral("pairing-outsider"));
  auto forged = QDBusMessage::createMethodCall(app.baseService(), manager.path,
      QStringLiteral("org.bluez.Agent1"), QStringLiteral("RequestConfirmation"));
  forged.setArguments({device, uint(555555)});
  QDBusPendingCallWatcher spoof(outsider.asyncCall(forged));
  QTRY_VERIFY(spoof.isFinished());
  QVERIFY(spoof.isError());
  QVERIFY(!agent.pending());
  forged = QDBusMessage::createMethodCall(app.baseService(), manager.path,
      QStringLiteral("org.bluez.Agent1"), QStringLiteral("resolve"));
  forged.setArguments({qulonglong(1), true});
  QDBusPendingCallWatcher remoteDecision(outsider.asyncCall(forged));
  QTRY_VERIFY(remoteDecision.isFinished());
  QVERIFY(remoteDecision.isError());
  QDBusConnection::disconnectFromBus(QStringLiteral("pairing-outsider"));

  QDBusPendingCallWatcher pending(call(QStringLiteral("RequestConfirmation"), {device, uint(333333)}));
  QTRY_VERIFY(agent.pending());
  const quint64 oldId = agent.requestId();
  QVERIFY(bluez.unregisterService(QStringLiteral("org.bluez")));
  QTRY_VERIFY(!agent.pending());
  QTRY_VERIFY(pending.isFinished());
  QVERIFY(pending.isError());
  const int before = manager.defaults;
  QVERIFY(bluez.registerService(QStringLiteral("org.bluez")));
  QTRY_COMPARE(manager.defaults, before + 1);
  QDBusPendingCallWatcher recovered(call(QStringLiteral("RequestConfirmation"), {device, uint(444444)}));
  QTRY_VERIFY(agent.pending());
  agent.resolve(oldId, true);
  QVERIFY(agent.pending());
  agent.resolve(agent.requestId(), true);
  QTRY_VERIFY(recovered.isFinished());
  QVERIFY(!recovered.isError());
}

void TestPairing::adapterAvailabilityAndPairedReconnect() {
  BluetoothClient bluetooth(app);
  const QString adapter = QStringLiteral("/org/bluez/hci0");
  const QString devicePath = adapter + QStringLiteral("/dev_FF");
  const QVariant device = QVariant::fromValue(QDBusObjectPath(devicePath));
  bluetooth.bluezObjectAddedForTest(adapter, QStringLiteral("org.bluez.Adapter1"),
                                  {{QStringLiteral("Powered"), true}});
  bluetooth.bluezObjectAddedForTest(devicePath, QStringLiteral("org.bluez.Device1"),
      {{QStringLiteral("Paired"), true}, {QStringLiteral("Alias"), QStringLiteral("Saved Phone")}});
  QDBusPendingCallWatcher inactive(call(QStringLiteral("RequestConfirmation"), {device, uint(123)}));
  QTRY_VERIFY(inactive.isFinished());
  QVERIFY(inactive.isError()); // powered radio alone is not permission to pair
  const int before = manager.defaults;
  bluetooth.setPairingEnabled(true);
  QTRY_COMPARE(manager.defaults, before + 1);
  QDBusPendingCallWatcher reconnect(call(QStringLiteral("AuthorizeService"), {device, QStringLiteral("audio")}));
  QTRY_VERIFY(reconnect.isFinished());
  QVERIFY(!reconnect.isError());
  QVERIFY(!bluetooth.pairing()->pending());

  bluetooth.bluezPropertyChangedForTest(devicePath, QStringLiteral("org.bluez.Device1"), {},
                                       {QStringLiteral("Paired")});
  QDBusPendingCallWatcher noLongerPaired(call(QStringLiteral("AuthorizeService"), {device, QStringLiteral("audio")}));
  QTRY_VERIFY(noLongerPaired.isFinished());
  QVERIFY(noLongerPaired.isError());
  QDBusPendingCallWatcher pending(call(QStringLiteral("RequestConfirmation"), {device, uint(123)}));
  QTRY_VERIFY(bluetooth.pairing()->pending());
  QCOMPARE(bluetooth.pairing()->deviceName(), QStringLiteral("Saved Phone"));
  bluetooth.bluezPropertyChangedForTest(adapter, QStringLiteral("org.bluez.Adapter1"),
                                       {{QStringLiteral("Powered"), false}});
  QTRY_VERIFY(pending.isFinished());
  QVERIFY(pending.isError());
  QVERIFY(!bluetooth.pairing()->pending());
}

QTEST_GUILESS_MAIN(TestPairing)
#include "tst_pairing.moc"
