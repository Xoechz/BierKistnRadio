#include "VolumeController.h"

#include <QProcess>
#include <QRegularExpression>

#include <algorithm>

namespace {
constexpr int kPollIntervalMs = 1000;
}

VolumeController::VolumeController(QObject *parent) : QObject(parent) {
  m_runner = [](const QStringList &args,
                const std::function<void(const QByteArray &output)> &onFinished) {
    auto *proc = new QProcess;
    QObject::connect(proc, &QProcess::finished, proc,
                     [proc, onFinished](int, QProcess::ExitStatus) {
                       onFinished(proc->readAllStandardOutput());
                       proc->deleteLater();
                     });
    proc->start(QStringLiteral("wpctl"), args);
  };

  m_pollTimer.setInterval(kPollIntervalMs);
  connect(&m_pollTimer, &QTimer::timeout, this, &VolumeController::pollVolume);
  m_pollTimer.start();
}

int VolumeController::volume() const { return m_volume; }

void VolumeController::setVolume(int percent) {
  percent = std::clamp(percent, 0, m_maxVolumePercent);

  if (m_volume == percent) {
    return;
  }

  m_volume = percent;
  emit volumeChanged();

  ++m_writeGen; // invalidate any in-flight poll read
  m_runner(QStringList{QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                       QString::number(percent) + QStringLiteral("%")},
           [](const QByteArray &) {});
}

void VolumeController::increaseVolume() { setVolume(m_volume + 5); }

void VolumeController::decreaseVolume() { setVolume(m_volume - 5); }

int VolumeController::parseVolume(const QByteArray &output) {
  const QRegularExpression re(QStringLiteral("^Volume:\\s*([0-9]+(?:\\.[0-9]+)?)"));
  const QRegularExpressionMatch match = re.match(QString::fromUtf8(output));
  if (!match.hasMatch()) {
    return -1;
  }

  bool ok = false;
  const double ratio = match.captured(1).toDouble(&ok);
  if (!ok) {
    return -1;
  }

  return std::clamp(qRound(ratio * 100.0), 0, 150);
}

void VolumeController::pollVolume() {
  const quint64 genAtIssue = m_writeGen;
  m_runner(QStringList{QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")},
           [this, genAtIssue](const QByteArray &output) {
             if (genAtIssue != m_writeGen) {
               return; // a write landed after this read was issued; discard stale
             }
             const int parsed = parseVolume(output);
             if (parsed < 0 || parsed == m_volume) {
               return;
             }
             m_volume = parsed;
             emit volumeChanged();
           });
}

void VolumeController::setCommandRunnerForTest(const CommandRunner &runner) { m_runner = runner; }

void VolumeController::pollNowForTest() { pollVolume(); }