#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <qqmlintegration.h>

class ArtCache : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(ArtCache)

  Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)

public:
  explicit ArtCache(QObject *parent = nullptr);

  QString cacheDir() const;

  Q_INVOKABLE QUrl cacheArt(const QUrl &artUrl, const QString &key);
  Q_INVOKABLE void clearCache();

signals:
  void artCached(const QString &key, const QUrl &cachedUrl);

private:
  QString m_cacheDir;
};
