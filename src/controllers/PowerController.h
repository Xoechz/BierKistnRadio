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
  explicit PowerController(QObject *parent = nullptr);
  QString errorMessage() const;
  bool busy() const;

  Q_INVOKABLE void reboot();
  Q_INVOKABLE void shutdown();
  Q_INVOKABLE void clearError();
  using CommandRunner = std::function<void(const QString &action,
                                            const std::function<void(const QString &error)> &done)>;
  void setCommandRunnerForTest(const CommandRunner &runner);

signals:
  void errorMessageChanged();
  void busyChanged();
  void commandSucceeded();

private:
  void run(const QString &action);
  void setErrorMessage(const QString &message);
  QString m_errorMessage;
  bool m_busy = false;
  CommandRunner m_runner;
};
