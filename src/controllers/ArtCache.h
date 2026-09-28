#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QHash>
#include <qqmlintegration.h>

class QNetworkAccessManager;
class QNetworkReply;

class ArtCache : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(ArtCache)

  Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)

public:
  explicit ArtCache(QObject *parent = nullptr);
  ArtCache(QNetworkAccessManager *manager, const QString &cacheDir,
           QObject *parent = nullptr);

  QString cacheDir() const;

  // Returns a cached file URL immediately, or an empty URL while downloading.
  // artCached(key, url) fires on completion; an empty url means failure.
  Q_INVOKABLE QUrl cacheArt(const QUrl &artUrl, const QString &key);
  Q_INVOKABLE void clearCache();

signals:
  void artCached(const QString &key, const QUrl &cachedUrl);

private:
  void prune();

  QString m_cacheDir;
  QNetworkAccessManager *m_manager;
  QHash<QString, QNetworkReply *> m_pending;
  QHash<QString, QStringList> m_waiters;
  quint64 m_generation = 0;
};
