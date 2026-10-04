#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QTimer>
#include <functional>

// The kiosk user's systemd manager, not MPRIS, is the source-readiness boundary.
// Polling only observes state; it never starts or restarts a failed service.
class SpotifyServiceClient : public QObject {
  Q_OBJECT
public:
  /// Start passive 500 ms service polling; construction does not start spotifyd.
  /// @param bus Kiosk user's session bus. @param parent Optional owner. No return.
  explicit SpotifyServiceClient(const QDBusConnection &bus, QObject *parent = nullptr);
  /// @return Whether the cached service observation is currently validated.
  bool known() const { return m_known; }
  /// @return Whether cached ActiveState is active; check known() separately.
  bool running() const { return m_state == QStringLiteral("active"); }
  /// @return Whether a known observation reports failed startup/state.
  bool failed() const { return m_known && m_state == QStringLiteral("failed"); }
  /// @return Whether a known state is inactive or failed (outgoing process stopped).
  bool stopped() const {
    return m_known && (m_state == QStringLiteral("inactive") ||
                       m_state == QStringLiteral("failed"));
  }
  /// @return Classified status-read error, or empty after a valid observation.
  QString errorMessage() const { return m_error; }
  /// Submit one lifecycle job and invalidate pre-command reads.
  /// @param running True starts; false stops spotifyd.service.
  /// @param finished Callback taking empty acknowledgement or classified failure.
  /// @note No return; successful acknowledgement is not observed readiness.
  void requestRunning(bool running, const std::function<void(const QString &)> &finished);
  /// Load/read service state asynchronously, coalescing pending reads. No params/return.
  void refresh();

signals:
  /// Announce observation/error updates, even unchanged valid state. No params/return.
  void stateChanged();

private:
  /// Read Unit properties; only current-generation results can establish known state.
  /// @param generation Read token. No return; properties/errors update asynchronously.
  void readState(quint64 generation);
  /// @param error Raw read/protocol error to classify; marks state unknown. No return.
  void readFailed(const QString &error);
  QDBusConnection m_bus;
  QTimer m_poll;
  QString m_path;
  QString m_state;
  QString m_error;
  bool m_known = false;
  bool m_readPending = false;
  quint64 m_generation = 0;
};
