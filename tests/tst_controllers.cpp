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
  void testPlaybackControllerBluetoothTransitions();
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
