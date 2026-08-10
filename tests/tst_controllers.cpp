#include "ArtCache.h"
#include "BluetoothController.h"
#include "PlaybackController.h"
#include "VolumeController.h"
#include "WifiController.h"
#include <QtTest/QtTest>

class TestControllers : public QObject {
  Q_OBJECT

private slots:
  void testPlaybackControllerDefaults();
  void testWifiControllerDefaults();
  void testBluetoothControllerDefaults();
  void testVolumeControllerSetVolume();
  void testVolumeControllerClamping();
  void testArtCacheDirCreation();
};

void TestControllers::testPlaybackControllerDefaults() {
  PlaybackController c;
  QCOMPARE(c.title(), QString());
  QCOMPARE(c.artist(), QString());
  QCOMPARE(c.isSinkMode(), false);
  QCOMPARE(c.isStation(), false);
  QCOMPARE(c.isPlaying(), false);
  QCOMPARE(c.position(), qint64(0));
  QCOMPARE(c.duration(), qint64(0));
}

void TestControllers::testWifiControllerDefaults() {
  WifiController c;
  QCOMPARE(c.connected(), false);
  QCOMPARE(c.ssid(), QString());
  QCOMPARE(c.signalStrength(), 0);
  QCOMPARE(c.networks(), QStringList());
}

void TestControllers::testBluetoothControllerDefaults() {
  BluetoothController c;
  QCOMPARE(c.discoverable(), false);
  QCOMPARE(c.pairedDeviceName(), QString());
  QCOMPARE(c.devices(), QStringList());
}

void TestControllers::testVolumeControllerSetVolume() {
  VolumeController c;
  QCOMPARE(c.volume(), 50);

  c.setVolume(75);
  QCOMPARE(c.volume(), 75);
}

void TestControllers::testVolumeControllerClamping() {
  VolumeController c;
  c.setVolume(200);
  QCOMPARE(c.volume(), 150);

  c.setVolume(-10);
  QCOMPARE(c.volume(), 0);
}

void TestControllers::testArtCacheDirCreation() {
  ArtCache cache;
  QVERIFY(!cache.cacheDir().isEmpty());
  QVERIFY(QDir(cache.cacheDir()).exists());
}

QTEST_MAIN(TestControllers)
#include "tst_controllers.moc"
