#pragma once

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <functional>
#include <optional>
#include <qqmlintegration.h>

class VolumeController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(VolumeController)

  Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
  Q_PROPERTY(bool muted READ muted NOTIFY volumeChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
  /// Install bounded wpctl runner and start one-second reads. No return value.
  /// @param parent Optional QObject owner; initial cached volume is zero.
  explicit VolumeController(QObject *parent = nullptr);

  /// @return Cached requested/observed percentage in 0..150; writes are optimistic.
  int volume() const;
  /// @return Whether cached volume is zero, not the sink's separate mute flag.
  bool muted() const;
  /// @return Current read/write failure text, or empty when clear.
  QString errorMessage() const;

  /// Clamp, cache and asynchronously request default-sink volume. No return value.
  /// @param percent Integer percentage, clamped to 0..150; supersedes older reads.
  Q_INVOKABLE void setVolume(int percent);
  /// @param muted True zeroes volume; false restores last nonzero level or 10%.
  /// No return value; does not use wpctl set-mute.
  Q_INVOKABLE void setMuted(bool muted);
  /// Request +5 percentage points through normal clamping. No parameters/return.
  Q_INVOKABLE void increaseVolume();
  /// Request -5 percentage points through normal clamping. No parameters/return.
  Q_INVOKABLE void decreaseVolume();

  // Runs a `wpctl` command; an empty error indicates success. Injectable so
  // tests never need a live PipeWire/WirePlumber daemon.
  using CommandRunner = std::function<void(
      const QStringList &args,
      const std::function<void(const QByteArray &output, const QString &error)> &onFinished)>;

  // Parses `wpctl get-volume` output ("Volume: 0.65\n") into a percent
  // (0..150); returns -1 if the output is not parseable.
  /// @param output wpctl stdout, possibly with a trailing mute marker.
  /// @return Rounded/clamped 0..150 percentage, or -1 for unparseable leading ratio.
  static int parseVolume(const QByteArray &output);

  // Test hooks: swap the wpctl runner and trigger a poll synchronously.
  /// @param runner Replacement wpctl-arguments/completion callable. No return value.
  void setCommandRunnerForTest(const CommandRunner &runner);
  /// Trigger normal polling now; completion may remain async. No parameters/return.
  void pollNowForTest();

signals:
  /// Notify both volume and derived muted predicate. No parameters/return.
  void volumeChanged();
  /// Notify read/write diagnostic changes. No parameters/return.
  void errorMessageChanged();

private:
  /// Read the shared sink, reject pre-write replies and track external restore history.
  /// No parameters/return; failed-write errors clear only on suitable observed recovery.
  void pollVolume();
  /// @param message Error text or empty to clear; notify only on change. No return.
  void setErrorMessage(const QString &message);

  int m_volume = 0;
  int m_lastNonzeroVolume = 0;
  int m_maxVolumePercent = 150;
  quint64 m_writeGen = 0;
  QTimer m_pollTimer;
  CommandRunner m_runner;
  QString m_errorMessage;
  std::optional<int> m_failedVolume;
};
