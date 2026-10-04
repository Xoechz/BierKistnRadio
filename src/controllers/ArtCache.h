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
  /// Construct owned network manager and standard cache directory. No return value.
  /// @param parent Optional QObject owner.
  explicit ArtCache(QObject *parent = nullptr);
  /// Create/prune the supplied directory. No return; injected manager stays caller-owned.
  /// @param manager Non-null borrowed manager. @param cacheDir Artwork directory path.
  /// @param parent Optional QObject owner.
  ArtCache(QNetworkAccessManager *manager, const QString &cacheDir,
           QObject *parent = nullptr);

  /// @return Configured cache directory path; does not perform a filesystem query.
  QString cacheDir() const;

  // Returns a cached file URL immediately, or an empty URL while downloading.
  // artCached(key, url) fires on completion; an empty url means failure.
  /// Reuse/download URL-hashed supported artwork with bounded storage and transfer.
  /// @param artUrl HTTP(S) source. @param key Caller correlation key, not filename.
  /// @return Local cache-hit URL, otherwise empty pending/unsupported/failure result.
  /// Pending downloads complete through artCached; old generations are discarded.
  Q_INVOKABLE QUrl cacheArt(const QUrl &artUrl, const QString &key);
  /// Abort/forget pending waiters and remove cached image files. No params/return.
  Q_INVOKABLE void clearCache();

signals:
  /// Publish queued download completion; cache hits return directly instead.
  /// @param key Waiting caller key. @param cachedUrl Local URL or empty failure.
  /// No return value; consumers must reject keys obsolete for their current track.
  void artCached(const QString &key, const QUrl &cachedUrl);

private:
  /// Delete oldest image files until counted data is <=100 MiB, as deletion succeeds.
  /// No parameters/return; cache hits refresh modification time for reuse ordering.
  void prune();

  QString m_cacheDir;
  QNetworkAccessManager *m_manager;
  QHash<QString, QNetworkReply *> m_pending;
  QHash<QString, QStringList> m_waiters;
  quint64 m_generation = 0;
};
