#include "ArtCache.h"
#include "BluetoothClient.h"
#include "PlaybackController.h"
#include "SpotifyClient.h"
#include "VolumeController.h"
#include "WifiController.h"
#include <QtTest/QtTest>

class TestControllers : public QObject {
  Q_OBJECT

private slots:
  void testPlaybackControllerDefaults();
  void testWifiControllerDefaults();
  void testBluetoothClientDefaults();
  void testSpotifyClientDefaults();
  void testVolumeControllerSetVolume();
  void testVolumeControllerClamping();
  void testArtCacheDirCreation();
};

void TestControllers::testPlaybackControllerDefaults() {
  PlaybackController c;
  QCOMPARE(c.playbackState(), PlaybackController::SpotifyUnavailable);
  QCOMPARE(c.isBluetoothActive(), false);
  QVERIFY(c.spotify() != nullptr);
  QVERIFY(c.bluetooth() != nullptr);
}

void TestControllers::testWifiControllerDefaults() {
  WifiController c;
  QCOMPARE(c.connected(), false);
  QCOMPARE(c.ssid(), QString());
  QCOMPARE(c.signalStrength(), 0);
  QCOMPARE(c.networks(), QStringList());
}

void TestControllers::testBluetoothClientDefaults() {
  BluetoothClient c;
  QCOMPARE(c.connectedDeviceName(), QString());
  QCOMPARE(c.takeoverPending(), false);
  QCOMPARE(c.statusPublished(), false);
  QCOMPARE(c.trackPublished(), false);
  QCOMPARE(c.muted(), false);
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
