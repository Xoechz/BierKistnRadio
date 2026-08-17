#pragma once

#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <functional>
#include <qqmlintegration.h>

class VolumeController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(VolumeController)

  Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)

public:
  explicit VolumeController(QObject *parent = nullptr);

  int volume() const;

  Q_INVOKABLE void setVolume(int percent);
  Q_INVOKABLE void increaseVolume();
  Q_INVOKABLE void decreaseVolume();

  // Runs a `wpctl` command; `onFinished` receives stdout. Injectable so
  // tests never need a live PipeWire/WirePlumber daemon.
  using CommandRunner = std::function<void(
      const QStringList &args,
      const std::function<void(const QByteArray &output)> &onFinished)>;

  // Parses `wpctl get-volume` output ("Volume: 0.65\n") into a percent
  // (0..150); returns -1 if the output is not parseable.
  static int parseVolume(const QByteArray &output);

  // Test hooks: swap the wpctl runner and trigger a poll synchronously.
  void setCommandRunnerForTest(const CommandRunner &runner);
  void pollNowForTest();

signals:
  void volumeChanged();

private:
  void pollVolume();

  int m_volume = 0;
  int m_maxVolumePercent = 150;
  quint64 m_writeGen = 0;
  QTimer m_pollTimer;
  CommandRunner m_runner;
};