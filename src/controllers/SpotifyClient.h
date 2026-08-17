#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QVariantMap>
#include <qqmlintegration.h>

class SpotifyClient : public QObject {
  Q_OBJECT
  QML_NAMED_ELEMENT(SpotifyClient)
  QML_UNCREATABLE("SpotifyClient is owned by PlaybackController")

  Q_PROPERTY(QString title READ title NOTIFY titleChanged)
  Q_PROPERTY(QString artist READ artist NOTIFY artistChanged)
  Q_PROPERTY(QString album READ album NOTIFY albumChanged)
  Q_PROPERTY(QString artUrl READ artUrl NOTIFY artUrlChanged)
  Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
  Q_PROPERTY(bool isSpotifyPlaying READ isSpotifyPlaying NOTIFY
                 isSpotifyPlayingChanged)

public:
  explicit SpotifyClient(QObject *parent = nullptr);

  QString title() const;
  QString artist() const;
  QString album() const;
  QString artUrl() const;
  qint64 position() const;
  qint64 duration() const;
  bool isSpotifyPlaying() const;

  bool hasTrack() const;
  bool isAvailable() const;

  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void seek(qint64 positionMs);

  // Test hook: lets unit tests drive presence/availability the way the
  // MPRIS2 D-Bus subscription updates it at runtime, without a live bus.
  void setAvailableForTest(bool available);
  void setHasTrackForTest(bool hasTrack);

signals:
  void titleChanged();
  void artistChanged();
  void albumChanged();
  void artUrlChanged();
  void positionChanged();
  void durationChanged();
  void isSpotifyPlayingChanged();
  void hasTrackChanged();
  void availableChanged();

private:
  QString m_title;
  QString m_artist;
  QString m_album;
  QString m_artUrl;
  qint64 m_position = 0;
  qint64 m_duration = 0;
  bool m_isSpotifyPlaying = false;
  bool m_hasTrack = false;
  bool m_available = false;
  QString m_mprisService;
  QDBusObjectPath m_trackId;

  void discoverServices();
  void onServiceOwnerChanged(const QString &name, const QString &oldOwner,
                             const QString &newOwner);
  void subscribeToMpris();
  void unsubscribeFromMpris();
  void fetchInitialMprisState();
  void onMprisPropertiesChanged(const QString &interface,
                                const QVariantMap &changed,
                                const QStringList &invalidated);
  void updateFromMetadata(const QVariantMap &metadata);
  void updatePlaybackStatus(const QString &status);
  void setTrackPresence(bool present);
  void setAvailable(bool available);
};
