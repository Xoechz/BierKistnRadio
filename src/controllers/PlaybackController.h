#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

#include "BluetoothClient.h"
#include "SpotifyClient.h"
#include "SpotifyServiceClient.h"

class ReleaseDateClient;

class PlaybackController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(PlaybackController)

  Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY
                 playbackStateChanged)
  Q_PROPERTY(bool isBluetoothActive READ isBluetoothActive NOTIFY
                 isBluetoothActiveChanged)
  Q_PROPERTY(SpotifyClient *spotify READ spotify CONSTANT)
  Q_PROPERTY(BluetoothClient *bluetooth READ bluetooth CONSTANT)
  Q_PROPERTY(QString releaseDate READ releaseDate NOTIFY releaseDateChanged)
  Q_PROPERTY(bool switching READ switching NOTIFY sourceStatusChanged)
  Q_PROPERTY(bool sourceReady READ sourceReady NOTIFY sourceStatusChanged)
  Q_PROPERTY(QString sourceError READ sourceError NOTIFY sourceStatusChanged)

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
  PlaybackController(const QDBusConnection &sessionBus, const QDBusConnection &systemBus,
                     QObject *parent = nullptr, int transitionTimeoutMs = 10000);

  PlaybackState playbackState() const;
  bool isBluetoothActive() const;
  SpotifyClient *spotify() const;
  BluetoothClient *bluetooth() const;
  QString releaseDate() const;
  bool switching() const { return m_switching; }
  bool sourceReady() const { return m_sourceReady; }
  QString sourceError() const { return m_sourceError; }

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void seek(qint64 positionMs);
  Q_INVOKABLE void switchToBluetooth();
  Q_INVOKABLE void switchToSpotify();
  Q_INVOKABLE void retrySource();

signals:
  void playbackStateChanged();
  void isBluetoothActiveChanged();
  void releaseDateChanged();
  void sourceStatusChanged();

private:
  PlaybackState m_playbackState = SpotifyWaiting;
  SpotifyClient *m_spotify = nullptr;
  BluetoothClient *m_bluetooth = nullptr;
  ReleaseDateClient *m_releaseDates = nullptr;
  SpotifyServiceClient *m_service = nullptr;
  bool m_bluetoothSelected = false;
  bool m_switching = false;
  bool m_sourceReady = false;
  QString m_sourceError;
  quint64 m_attempt = 0;
  bool m_outgoingIssued = false;
  bool m_incomingIssued = false;
  bool m_commandPending = false;
  QTimer m_transitionTimer;
  void beginTransition(bool bluetooth);
  void advanceTransition();
  void observeSource();
  void failTransition(const QString &error);
  bool requestedReady() const;
  void refreshPlaybackState();

  void onSpotifyChanged();
  void onBluetoothChanged();
  void refreshReleaseDate();
  void setPlaybackState(PlaybackState next);
};
