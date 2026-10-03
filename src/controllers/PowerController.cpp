#include "PowerController.h"
#include "ControllerError.h"

#include <QProcess>
#include <QTimer>
#include <memory>

PowerController::PowerController(QObject *parent) : QObject(parent) {
  m_runner = [this](const QString &action, const std::function<void(const QString &)> &done) {
    auto *proc = new QProcess(this);
    auto completed = std::make_shared<bool>(false);
    auto *timeout = new QTimer(proc);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, proc, [proc, done, completed]() {
      if (*completed) {
        return;
      }
      *completed = true;
      proc->kill();
      done(QStringLiteral("timed out"));
      proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, proc,
            [proc, done, completed](QProcess::ProcessError error) {
              if (error != QProcess::FailedToStart || *completed) {
                return;
              }
              *completed = true;
              done(QStringLiteral("service not running: ") + proc->errorString());
              proc->deleteLater();
            });
    connect(proc, &QProcess::finished, proc,
            [proc, done, completed](int exitCode, QProcess::ExitStatus status) {
              if (*completed) {
                return;
              }
              *completed = true;
              const QString detail = QString::fromUtf8(proc->readAllStandardError()).trimmed();
              done(status == QProcess::NormalExit && exitCode == 0
                       ? QString()
                       : (detail.isEmpty() ? QStringLiteral("exit code %1").arg(exitCode)
                                           : detail));
              proc->deleteLater();
            });
    proc->start(QStringLiteral("systemctl"), {action});
    timeout->start(5000);
  };
}

QString PowerController::errorMessage() const { return m_errorMessage; }

bool PowerController::busy() const { return m_busy; }

void PowerController::setErrorMessage(const QString &message) {
  if (m_errorMessage == message) {
    return;
  }
  m_errorMessage = message;
  emit errorMessageChanged();
}

void PowerController::setCommandRunnerForTest(const CommandRunner &runner) { m_runner = runner; }

void PowerController::clearError() { setErrorMessage({}); }

void PowerController::run(const QString &action) {
  if (m_busy) {
    return;
  }
  setErrorMessage({});
  m_busy = true;
  emit busyChanged();
  m_runner(action, [this, action](const QString &error) {
    m_busy = false;
    emit busyChanged();
    if (!error.isEmpty()) {
      setErrorMessage(controllerErrorText(action == QStringLiteral("reboot")
                                              ? QStringLiteral("Reboot")
                                              : QStringLiteral("Shutdown"), error));
      return;
    }
    setErrorMessage({});
    emit commandSucceeded();
  });
}

void PowerController::reboot() {
  run(QStringLiteral("reboot"));
}

void PowerController::shutdown() {
  run(QStringLiteral("poweroff"));
}
