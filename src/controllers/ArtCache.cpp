#include "ArtCache.h"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

ArtCache::ArtCache(QObject *parent)
    : QObject(parent), m_cacheDir(QStandardPaths::writableLocation(
                                      QStandardPaths::CacheLocation) +
                                  "/art") {
  QDir().mkpath(m_cacheDir);
}

QString ArtCache::cacheDir() const { return m_cacheDir; }

QUrl ArtCache::cacheArt(const QUrl &artUrl, const QString &key) {
  Q_UNUSED(key)
  return artUrl;
}

void ArtCache::clearCache() {
  QDir dir(m_cacheDir);
  dir.setNameFilters({"*.jpg", "*.png", "*.jpeg", "*.webp"});

  for (const auto &file : dir.entryList(QDir::Files)) {
    dir.remove(file);
  }
}
