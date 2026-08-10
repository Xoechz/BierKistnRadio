#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

class PlaybackController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(PlaybackController)

  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(QString artist READ artist NOTIFY artistChanged)
  Q_PROPERTY(QString album READ album NOTIFY albumChanged)
  Q_PROPERTY(QString artUrl READ artUrl NOTIFY artUrlChanged)
  Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
  Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
  Q_PROPERTY(bool isStation READ isStation NOTIFY isStationChanged)
  Q_PROPERTY(bool isSinkMode READ isSinkMode NOTIFY isSinkModeChanged)
  Q_PROPERTY(QString pairedDeviceName READ pairedDeviceName NOTIFY
                 pairedDeviceNameChanged)

public:
  explicit PlaybackController(QObject *parent = nullptr);

  QString title() const;
  QString artist() const;
  QString album() const;
  QString artUrl() const;
  qint64 position() const;
  qint64 duration() const;
  bool isPlaying() const;
  bool isStation() const;
  bool isSinkMode() const;
  QString pairedDeviceName() const;

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void seek(qint64 positionMs);

signals:
  void titleChanged();
  void artistChanged();
  void albumChanged();
  void artUrlChanged();
  void positionChanged();
  void durationChanged();
  void isPlayingChanged();
  void isStationChanged();
  void isSinkModeChanged();
  void pairedDeviceNameChanged();

private:
  QString m_title;
  QString m_artist;
  QString m_album;
  QString m_artUrl;
  qint64 m_position = 0;
  qint64 m_duration = 0;
  bool m_isPlaying = false;
  bool m_isStation = false;
  bool m_isSinkMode = false;
  QString m_pairedDeviceName;
};
