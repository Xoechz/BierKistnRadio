#pragma once

#include <QObject>
#include <functional>
#include <qqmlintegration.h>

class PowerController : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_NAMED_ELEMENT(PowerController)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
  /// Install a five-second bounded systemctl runner. @param parent Owner; no return.
  explicit PowerController(QObject *parent = nullptr);
  /// @return Cached failure text, or empty when clear.
  QString errorMessage() const;
  /// @return Whether a command is pending (prevents overlapping requests).
  bool busy() const;

  /// Request reboot after caller confirmation. No params/return; success is async.
  Q_INVOKABLE void reboot();
  /// Request poweroff after caller confirmation. No params/return; success is async.
  Q_INVOKABLE void shutdown();
  /// Clear displayed error, not an in-flight command. No parameters/return.
  Q_INVOKABLE void clearError();
  /// Callable: action string and done(error) -> void; empty error acknowledges exit success.
  using CommandRunner = std::function<void(const QString &action,
                                            const std::function<void(const QString &error)> &done)>;
  /// @param runner Replacement action/completion runner, avoiding real power changes.
  /// No return value.
  void setCommandRunnerForTest(const CommandRunner &runner);

signals:
  /// Notify cached error changes. No parameters/return.
  void errorMessageChanged();
  /// Notify command-pending changes. No parameters/return.
  void busyChanged();
  /// Notify successful command exit, not physical power completion. No params/return.
  void commandSucceeded();

private:
  /// Dispatch one systemctl command unless busy; publish classified error/success later.
  /// @param action reboot/poweroff action string. No return value.
  void run(const QString &action);
  /// @param message Error text or empty to clear; notify only on change. No return.
  void setErrorMessage(const QString &message);
  QString m_errorMessage;
  bool m_busy = false;
  CommandRunner m_runner;
};
