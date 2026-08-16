#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDBusObjectPath>
#include <qqmlintegration.h>

class BluetoothClient : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(BluetoothClient)
  QML_UNCREATABLE("BluetoothClient is owned by PlaybackController")

  Q_PROPERTY(QString connectedDeviceName READ connectedDeviceName NOTIFY
                 connectedDeviceNameChanged)
  Q_PROPERTY(bool takeoverPending READ takeoverPending NOTIFY
                 takeoverPendingChanged)
  Q_PROPERTY(bool statusPublished READ statusPublished NOTIFY
                 statusPublishedChanged)
  Q_PROPERTY(bool trackPublished READ trackPublished NOTIFY
                 trackPublishedChanged)
  Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackMetadataChanged)
  Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY trackMetadataChanged)
  Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY trackMetadataChanged)
  Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY trackMetadataChanged)
  Q_PROPERTY(bool positionPublished READ positionPublished NOTIFY
                 positionPublishedChanged)
  Q_PROPERTY(bool isBluetoothPlaying READ isBluetoothPlaying NOTIFY
                 statusChanged)
  Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)

public:
  enum TakeoverChoice { KeepCurrent, SwitchToNew };
  Q_ENUM(TakeoverChoice)

  explicit BluetoothClient(QObject *parent = nullptr);

  QString connectedDeviceName() const;
  bool takeoverPending() const;

  bool statusPublished() const;
  bool trackPublished() const;
  QString trackTitle() const;
  QString trackArtist() const;
  QString trackAlbum() const;
  qint64 position() const;
  qint64 duration() const;
  bool positionPublished() const;
  bool isBluetoothPlaying() const;
  bool muted() const;

  Q_INVOKABLE void resolveTakeover(TakeoverChoice choice);
  Q_INVOKABLE void ensureDiscoverable();

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void setMuted(bool muted);

signals:
  void connectedDeviceNameChanged();
  void takeoverPendingChanged();
  void statusPublishedChanged();
  void trackPublishedChanged();
  void trackMetadataChanged();
  void positionChanged();
  void positionPublishedChanged();
  void statusChanged();
  void mutedChanged();

private:
  QString m_connectedDeviceName;
  bool m_takeoverPending = false;
  bool m_statusPublished = false;
  bool m_trackPublished = false;
  QString m_trackTitle;
  QString m_trackArtist;
  QString m_trackAlbum;
  qint64 m_position = 0;
  qint64 m_duration = 0;
  bool m_positionPublished = false;
  bool m_isBluetoothPlaying = false;
  bool m_muted = false;
};
