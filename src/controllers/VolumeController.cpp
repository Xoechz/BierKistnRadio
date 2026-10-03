#include "VolumeController.h"
#include "ControllerError.h"

#include <QProcess>
#include <QRegularExpression>

#include <algorithm>
#include <memory>

namespace {
constexpr int kPollIntervalMs = 1000;
}

VolumeController::VolumeController(QObject *parent) : QObject(parent) {
  m_runner = [this](const QStringList &args,
                 const std::function<void(const QByteArray &, const QString &)> &onFinished) {
    auto *proc = new QProcess(this);
    auto completed = std::make_shared<bool>(false);
    auto *timeout = new QTimer(proc);
    timeout->setSingleShot(true);
    QObject::connect(timeout, &QTimer::timeout, proc, [proc, onFinished, completed]() {
      if (*completed) {
        return;
      }
      *completed = true;
      proc->kill();
      onFinished({}, QStringLiteral("timed out"));
      proc->deleteLater();
    });
    QObject::connect(proc, &QProcess::errorOccurred, proc,
                     [proc, onFinished, completed](QProcess::ProcessError error) {
                       if (error != QProcess::FailedToStart || *completed) {
                         return;
                       }
                       *completed = true;
                       onFinished({}, QStringLiteral("service not running: ") + proc->errorString());
                       proc->deleteLater();
                     });
    QObject::connect(proc, &QProcess::finished, proc,
                      [proc, onFinished, completed](int exitCode, QProcess::ExitStatus status) {
                        if (*completed) {
                          return;
                        }
                        *completed = true;
                        const QString detail = QString::fromUtf8(proc->readAllStandardError()).trimmed();
                        onFinished(proc->readAllStandardOutput(),
                                   status == QProcess::NormalExit && exitCode == 0
                                       ? QString()
                                       : (detail.isEmpty() ? QStringLiteral("exit code %1").arg(exitCode)
                                                           : detail));
                        proc->deleteLater();
                      });
    proc->start(QStringLiteral("wpctl"), args);
    timeout->start(5000);
  };

  m_pollTimer.setInterval(kPollIntervalMs);
  connect(&m_pollTimer, &QTimer::timeout, this, &VolumeController::pollVolume);
  m_pollTimer.start();
}

int VolumeController::volume() const { return m_volume; }

bool VolumeController::muted() const { return m_volume == 0; }

QString VolumeController::errorMessage() const { return m_errorMessage; }

void VolumeController::setErrorMessage(const QString &message) {
  if (m_errorMessage == message) {
    return;
  }
  m_errorMessage = message;
  emit errorMessageChanged();
}

void VolumeController::setVolume(int percent) {
  percent = std::clamp(percent, 0, m_maxVolumePercent);

  if (m_volume == percent && m_errorMessage.isEmpty()) {
    return;
  }

  if (percent > 0) {
    m_lastNonzeroVolume = percent;
  }
  m_volume = percent;
  emit volumeChanged();

  ++m_writeGen; // invalidate any in-flight poll read
  const quint64 genAtIssue = m_writeGen;
  m_runner(QStringList{QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                        QString::number(percent) + QStringLiteral("%")},
            [this, genAtIssue, percent](const QByteArray &, const QString &error) {
              if (genAtIssue != m_writeGen) {
                return;
              }
              m_failedVolume = error.isEmpty() ? std::nullopt : std::optional<int>(percent);
              setErrorMessage(error.isEmpty() ? QString()
                                              : controllerErrorText(QStringLiteral("Volume change"), error));
            });
}

void VolumeController::setMuted(bool muted) {
  if (muted) {
    setVolume(0);
  } else if (m_volume == 0) {
    setVolume(m_lastNonzeroVolume > 0 ? m_lastNonzeroVolume : 10);
  }
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
            [this, genAtIssue](const QByteArray &output, const QString &error) {
              if (genAtIssue != m_writeGen) {
                return; // a write landed after this read was issued; discard stale
              }
              if (!error.isEmpty()) {
                setErrorMessage(controllerErrorText(QStringLiteral("Volume read"), error));
                return;
              }
              const int parsed = parseVolume(output);
              if (parsed < 0) {
                setErrorMessage(QStringLiteral("Volume read failed: invalid wpctl output"));
                return;
              }
              if (!m_failedVolume || parsed == *m_failedVolume) {
                m_failedVolume.reset();
                setErrorMessage({});
              }
              if (parsed == m_volume) {
                return;
              }
              if (parsed > 0) {
                m_lastNonzeroVolume = parsed;
              }
              m_volume = parsed;
             emit volumeChanged();
           });
}

void VolumeController::setCommandRunnerForTest(const CommandRunner &runner) { m_runner = runner; }

void VolumeController::pollNowForTest() { pollVolume(); }
