#include "ArtCache.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>

#include <memory>

namespace {
constexpr qint64 kMaxCacheBytes = 100LL * 1024 * 1024;
constexpr qint64 kMaxDownloadBytes = 10LL * 1024 * 1024;
const QStringList kImageFilters{QStringLiteral("*.jpg"), QStringLiteral("*.png"),
                                QStringLiteral("*.webp")};

QString fileId(const QUrl &url) {
  return QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(),
                                                      QCryptographicHash::Sha256)
                                 .toHex());
}
} // namespace

ArtCache::ArtCache(QObject *parent)
    : ArtCache(new QNetworkAccessManager, QStandardPaths::writableLocation(
                                              QStandardPaths::CacheLocation) +
                                              QStringLiteral("/art"),
               parent) {
  m_manager->setParent(this);
}

ArtCache::ArtCache(QNetworkAccessManager *manager, const QString &cacheDir,
                   QObject *parent)
    : QObject(parent), m_cacheDir(cacheDir), m_manager(manager) {
  QDir().mkpath(m_cacheDir);
  prune();
}

QString ArtCache::cacheDir() const { return m_cacheDir; }

QUrl ArtCache::cacheArt(const QUrl &artUrl, const QString &key) {
  if (artUrl.scheme() != QStringLiteral("https") &&
      artUrl.scheme() != QStringLiteral("http")) {
    return {};
  }

  const QString id = fileId(artUrl);
  for (const QString &extension : {QStringLiteral("jpg"), QStringLiteral("png"),
                                   QStringLiteral("webp")}) {
    const QString path = m_cacheDir + QLatin1Char('/') + id + QLatin1Char('.') +
                         extension;
    if (QFileInfo::exists(path)) {
      QFile file(path);
      if (file.open(QIODevice::ReadOnly)) {
        file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
      }
      return QUrl::fromLocalFile(path);
    }
  }

  if (m_pending.contains(id)) {
    m_waiters[id].append(key);
    return {};
  }

  QNetworkRequest request(artUrl);
  request.setTransferTimeout(15000);
  QNetworkReply *reply = m_manager->get(request);
  m_pending.insert(id, reply);
  m_waiters.insert(id, {key});
  const quint64 generation = m_generation;
  auto bytes = std::make_shared<QByteArray>();
  connect(reply, &QIODevice::readyRead, this, [reply, bytes]() {
    bytes->append(reply->readAll());
    if (bytes->size() > kMaxDownloadBytes) {
      reply->abort();
    }
  });
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, id, generation, bytes]() {
            reply->deleteLater();
            if (generation != m_generation) {
              return;
            }
            m_pending.remove(id);
            const QStringList keys = m_waiters.take(id);
            bytes->append(reply->readAll());
            QUrl cachedUrl;
            if (reply->error() == QNetworkReply::NoError &&
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200 &&
                !bytes->isEmpty() && bytes->size() <= kMaxDownloadBytes) {
              QBuffer buffer(bytes.get());
              buffer.open(QIODevice::ReadOnly);
              QImageReader reader(&buffer);
              const QByteArray format = reader.format().toLower();
              if (reader.canRead() &&
                  (format == "jpeg" || format == "jpg" || format == "png" ||
                   format == "webp")) {
                const QString extension = format == "jpeg" ? QStringLiteral("jpg")
                                                             : QString::fromLatin1(format);
                const QString path = m_cacheDir + QLatin1Char('/') + id +
                                     QLatin1Char('.') + extension;
                QSaveFile file(path);
                if (file.open(QIODevice::WriteOnly) &&
                    file.write(*bytes) == bytes->size() && file.commit()) {
                  cachedUrl = QUrl::fromLocalFile(path);
                  prune();
                }
              }
            }
            for (const QString &key : keys) {
              emit artCached(key, cachedUrl);
            }
          });
  return {};
}

void ArtCache::prune() {
  QDir dir(m_cacheDir);
  const QFileInfoList files = dir.entryInfoList(kImageFilters, QDir::Files,
                                                QDir::Time | QDir::Reversed);
  qint64 total = 0;
  for (const QFileInfo &file : files) {
    total += file.size();
  }
  for (const QFileInfo &file : files) {
    if (total <= kMaxCacheBytes) {
      break;
    }
    if (QFile::remove(file.absoluteFilePath())) {
      total -= file.size();
    }
  }
}

void ArtCache::clearCache() {
  ++m_generation;
  const auto replies = m_pending.values();
  m_pending.clear();
  m_waiters.clear();
  for (QNetworkReply *reply : replies) {
    reply->abort();
  }

  QDir dir(m_cacheDir);
  for (const QString &file : dir.entryList(kImageFilters, QDir::Files)) {
    dir.remove(file);
  }
}
