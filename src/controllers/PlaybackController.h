#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

class PlaybackController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(PlaybackController)

  Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(QString artist READ artist NOTIFY artistChanged)
  Q_PROPERTY(QString album READ album NOTIFY albumChanged)
  Q_PROPERTY(QString artUrl READ artUrl NOTIFY artUrlChanged)
  Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
  Q_PROPERTY(bool isSpotifyPlaying READ isSpotifyPlaying NOTIFY isSpotifyPlayingChanged)
  Q_PROPERTY(bool isBluetoothActive READ isBluetoothActive NOTIFY isBluetoothActiveChanged)
  Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName NOTIFY
                 pairedDeviceNameChanged)

public:
  enum PlaybackState {
    SpotifyUnavailable,
    SpotifyReady,
    SpotifyActive,
    BluetoothWaiting,
    BluetoothActive,
  };
  Q_ENUM(PlaybackState)

  explicit PlaybackController(QObject *parent = nullptr);

  PlaybackState playbackState() const;
  QString title() const;
  QString artist() const;
  QString album() const;
  QString artUrl() const;
  qint64 position() const;
  qint64 duration() const;
  bool isSpotifyPlaying() const;
  bool isBluetoothActive() const;
  QString pairedDeviceName() const;

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void seek(qint64 positionMs);
  Q_INVOKABLE void transferPlayback();
  Q_INVOKABLE void switchToBluetooth();
  Q_INVOKABLE void switchToSpotify();

signals:
  void playbackStateChanged();
  void titleChanged();
  void artistChanged();
  void albumChanged();
  void artUrlChanged();
  void positionChanged();
  void durationChanged();
  void isSpotifyPlayingChanged();
  void isBluetoothActiveChanged();
  void pairedDeviceNameChanged();

private:
  PlaybackState m_playbackState = SpotifyUnavailable;
  QString m_title;
  QString m_artist;
  QString m_album;
  QString m_artUrl;
  qint64 m_position = 0;
  qint64 m_duration = 0;
  bool m_isSpotifyPlaying = false;
  QString m_pairedDeviceName;
};
