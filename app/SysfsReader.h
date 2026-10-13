#pragma once

#include "DeviceSnapshot.h"
#include <QByteArray>
#include <QStringList>
#include <expected>

namespace SysfsReader {
[[nodiscard]] std::expected<QByteArray, ScanIssue> read(const QString &path, qsizetype limit);
[[nodiscard]] std::expected<QString, ScanIssue> text(const QString &path, qsizetype limit = 4096);
[[nodiscard]] std::expected<QStringList, ScanIssue> directories(const QString &path);
[[nodiscard]] QString identity(const QString &path, bool includeChanges = true);
} // namespace SysfsReader
