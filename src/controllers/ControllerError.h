#pragma once

#include <QString>

inline QString controllerErrorText(const QString &operation, const QString &error) {
  if (error.contains(QStringLiteral("AccessDenied"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("NotAuthorized"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("permission"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("privilege"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("authentication required"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("authoriz"), Qt::CaseInsensitive)) {
    return QStringLiteral("Permission denied — check system config");
  }
  if (error.contains(QStringLiteral("ServiceUnknown"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("NameHasNoOwner"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("NoSuchUnit"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("service not running"), Qt::CaseInsensitive)) {
    return operation + QStringLiteral(": service unavailable — check system config");
  }
  if (error.contains(QStringLiteral("NoReply"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("timed out"), Qt::CaseInsensitive) ||
      error.contains(QStringLiteral("timeout"), Qt::CaseInsensitive)) {
    return operation + QStringLiteral(": timed out — try again");
  }
  return operation + QStringLiteral(" failed: ") + error;
}
