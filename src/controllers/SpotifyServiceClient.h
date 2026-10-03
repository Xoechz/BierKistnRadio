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
  explicit SpotifyServiceClient(const QDBusConnection &bus, QObject *parent = nullptr);
  bool known() const { return m_known; }
  bool running() const { return m_state == QStringLiteral("active"); }
  bool failed() const { return m_known && m_state == QStringLiteral("failed"); }
  bool stopped() const {
    return m_known && (m_state == QStringLiteral("inactive") ||
                       m_state == QStringLiteral("failed"));
  }
  QString errorMessage() const { return m_error; }
  void requestRunning(bool running, const std::function<void(const QString &)> &finished);
  void refresh();

signals:
  void stateChanged();

private:
  void readState(quint64 generation);
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
