#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

#include "BluetoothClient.h"
#include "SpotifyClient.h"

class PlaybackController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(PlaybackController)

  Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
  Q_PROPERTY(bool isBluetoothActive READ isBluetoothActive NOTIFY
                 isBluetoothActiveChanged)
  Q_PROPERTY(SpotifyClient *spotify READ spotify CONSTANT)
  Q_PROPERTY(BluetoothClient *bluetooth READ bluetooth CONSTANT)

public:
  enum PlaybackState {
    SpotifyUnavailable,
    SpotifyWaiting,
    SpotifyActive,
    BluetoothWaiting,
    BluetoothActive,
  };
  Q_ENUM(PlaybackState)

  explicit PlaybackController(QObject *parent = nullptr);

  PlaybackState playbackState() const;
  bool isBluetoothActive() const;
  SpotifyClient *spotify() const;
  BluetoothClient *bluetooth() const;

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void seek(qint64 positionMs);
  Q_INVOKABLE void switchToBluetooth();
  Q_INVOKABLE void switchToSpotify();

signals:
  void playbackStateChanged();
  void isBluetoothActiveChanged();

private:
  PlaybackState m_playbackState = SpotifyUnavailable;
  SpotifyClient *m_spotify = nullptr;
  BluetoothClient *m_bluetooth = nullptr;

  void onSpotifyChanged();
  void onBluetoothChanged();
  void refreshSpotifyState();
  void setPlaybackState(PlaybackState next);
};
