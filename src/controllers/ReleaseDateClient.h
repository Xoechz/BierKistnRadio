#pragma once

#include <QCache>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;

// MusicBrainz recording search; the playback facade controls which track is
// current, so old replies may warm the cache but cannot replace the UI value.
class ReleaseDateClient : public QObject {
  Q_OBJECT

public:
  explicit ReleaseDateClient(QObject *parent = nullptr);
  ReleaseDateClient(QNetworkAccessManager *manager, const QUrl &endpoint,
                    QObject *parent = nullptr);

  QString releaseDate() const;
  void setTrack(const QString &title, const QString &firstArtist);
  void clearTrack();

signals:
  void releaseDateChanged();

private:
  void dispatch();
  void setReleaseDate(const QString &date);

  QNetworkAccessManager *m_manager;
  QUrl m_endpoint;
  QString m_key;
  QString m_title;
  QString m_artist;
  QString m_releaseDate;
  QCache<QString, QString> m_cache{128};
  QElapsedTimer m_lastRequest;
  QTimer m_dispatchTimer;
};
