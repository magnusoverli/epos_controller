#include "SysfsReader.h"
#include <QFile>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>

namespace {
ScanIssue failure(const QString &path, int error) {
    const auto code = error == ENOENT || error == ENODEV  ? ReadError::Missing
                      : error == EACCES || error == EPERM ? ReadError::PermissionDenied
                                                          : ReadError::ReadFailed;
    return {code, path, QString::fromLocal8Bit(std::strerror(error))};
}
} // namespace

std::expected<QByteArray, ScanIssue> SysfsReader::read(const QString &path, qsizetype limit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::unexpected(failure(path, errno));
    auto bytes = file.read(limit + 1);
    if (file.error() != QFileDevice::NoError)
        return std::unexpected(ScanIssue{ReadError::ReadFailed, path, file.errorString()});
    if (bytes.size() > limit)
        return std::unexpected(
            ScanIssue{ReadError::TooLarge, path, QStringLiteral("Metadata exceeds inspection limit")});
    if (bytes.isEmpty())
        return std::unexpected(ScanIssue{ReadError::Empty, path, QStringLiteral("Metadata is empty")});
    return bytes;
}

std::expected<QString, ScanIssue> SysfsReader::text(const QString &path, qsizetype limit) {
    auto bytes = read(path, limit);
    if (!bytes)
        return std::unexpected(bytes.error());
    const auto value = QString::fromUtf8(*bytes).trimmed();
    if (value.isEmpty())
        return std::unexpected(ScanIssue{ReadError::Empty, path, QStringLiteral("Metadata is empty")});
    return value;
}

std::expected<QStringList, ScanIssue> SysfsReader::directories(const QString &path) {
    const auto native = QFile::encodeName(path);
    struct Close {
        void operator()(DIR *dir) const { closedir(dir); }
    };
    std::unique_ptr<DIR, Close> directory(opendir(native.constData()));
    if (!directory)
        return std::unexpected(failure(path, errno));
    QStringList entries;
    errno = 0;
    while (auto *entry = readdir(directory.get())) {
        const QByteArray name(entry->d_name);
        if (name != "." && name != "..")
            entries.append(QFile::decodeName(name));
        errno = 0;
    }
    if (errno != 0)
        return std::unexpected(
            ScanIssue{ReadError::EnumerationFailed, path, QString::fromLocal8Bit(std::strerror(errno))});
    std::ranges::sort(entries);
    return entries;
}

QString SysfsReader::identity(const QString &path, bool includeChanges) {
    struct stat info{};
    if (::stat(QFile::encodeName(path).constData(), &info) != 0)
        return {};
    if (!includeChanges)
        return QStringLiteral("%1:%2")
            .arg(static_cast<qulonglong>(info.st_dev))
            .arg(static_cast<qulonglong>(info.st_ino));
    return QStringLiteral("%1:%2:%3:%4")
        .arg(static_cast<qulonglong>(info.st_dev))
        .arg(static_cast<qulonglong>(info.st_ino))
        .arg(static_cast<qlonglong>(info.st_ctim.tv_sec))
        .arg(static_cast<qlonglong>(info.st_ctim.tv_nsec));
}
