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
  explicit VolumeController(QObject *parent = nullptr);

  int volume() const;
  bool muted() const;
  QString errorMessage() const;

  Q_INVOKABLE void setVolume(int percent);
  Q_INVOKABLE void setMuted(bool muted);
  Q_INVOKABLE void increaseVolume();
  Q_INVOKABLE void decreaseVolume();

  // Runs a `wpctl` command; an empty error indicates success. Injectable so
  // tests never need a live PipeWire/WirePlumber daemon.
  using CommandRunner = std::function<void(
      const QStringList &args,
      const std::function<void(const QByteArray &output, const QString &error)> &onFinished)>;

  // Parses `wpctl get-volume` output ("Volume: 0.65\n") into a percent
  // (0..150); returns -1 if the output is not parseable.
  static int parseVolume(const QByteArray &output);

  // Test hooks: swap the wpctl runner and trigger a poll synchronously.
  void setCommandRunnerForTest(const CommandRunner &runner);
  void pollNowForTest();

signals:
  void volumeChanged();
  void errorMessageChanged();

private:
  void pollVolume();
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
