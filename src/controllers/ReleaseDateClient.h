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
  /// Construct owned network manager for MusicBrainz. @param parent Owner; no return.
  explicit ReleaseDateClient(QObject *parent = nullptr);
  /// Configure rate-limited lookup against an injected endpoint. No return value.
  /// @param manager Non-null borrowed manager. @param endpoint Recording search URL.
  /// @param parent Optional owner; the injected manager is not reparented.
  ReleaseDateClient(QNetworkAccessManager *manager, const QUrl &endpoint,
                    QObject *parent = nullptr);

  /// @return Date/unknown/no-connection display string, or empty pending/cleared state.
  QString releaseDate() const;
  /// Select lookup identity, reuse cache or schedule an asynchronous request. No return.
  /// @param title Loaded track title. @param firstArtist First artist, not joined text.
  void setTrack(const QString &title, const QString &firstArtist);
  /// Clear current identity/display and scheduled dispatch. No parameters/return.
  /// Already-sent replies may warm cache but cannot update the now-obsolete UI.
  void clearTrack();

signals:
  /// Notify display-value changes. No parameters/return.
  void releaseDateChanged();

private:
  /// Start a >=1-second-spaced, bounded request and publish only current-key replies.
  /// No parameters/return; network/parse results arrive through display notification.
  void dispatch();
  /// @param date Display value (including status/empty text); notify on change. No return.
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
