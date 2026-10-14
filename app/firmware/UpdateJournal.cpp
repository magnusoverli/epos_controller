#include "UpdateJournal.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <fcntl.h>
#include <unistd.h>

namespace Firmware {
UpdateJournal::UpdateJournal(QString directory, const QByteArray &unitHash)
    : m_directory(std::move(directory)), m_path(m_directory + '/' + QString::fromLatin1(unitHash) + ".json"),
      m_lock(m_path + ".lock") {}
Result<void> UpdateJournal::acquire() {
    if (!QDir().mkpath(m_directory))
        return std::unexpected(QStringLiteral("Cannot create firmware operation directory."));
    if (!m_lock.tryLock(0))
        return std::unexpected(
            QStringLiteral("Another firmware operation is already running for this headset."));
    if (QFile::exists(m_path)) {
        QFile previous(m_path);
        if (!previous.open(QIODevice::ReadOnly) || previous.size() > 16384)
            return std::unexpected(QStringLiteral("Cannot inspect the previous firmware operation record."));
        const auto record = QJsonDocument::fromJson(previous.readAll()).object();
        if (record["schemaVersion"] != 1 ||
            (record["state"] != "verified" && record["state"] != "not-started"))
            return std::unexpected(QStringLiteral("An earlier update may be incomplete. Native retry is "
                                                  "blocked; use EPOS recovery/support. Operation record: %1")
                                       .arg(m_path));
    }
    return {};
}
Result<void> UpdateJournal::write(const QJsonObject &record) {
    if (!m_lock.isLocked())
        return std::unexpected(QStringLiteral("Firmware journal lock is not held."));
    QSaveFile file(m_path);
    const auto data = QJsonDocument(record).toJson();
    if (!file.open(QIODevice::WriteOnly) ||
        !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(data) != data.size() || !file.flush() || ::fsync(file.handle()) != 0 || !file.commit())
        return std::unexpected(
            QStringLiteral("Cannot durably save the firmware operation record. Update stopped."));
    const int fd = ::open(QFile::encodeName(m_directory).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    const bool ok = fd >= 0 && ::fsync(fd) == 0;
    if (fd >= 0)
        ::close(fd);
    if (!ok)
        return std::unexpected(QStringLiteral("Cannot synchronize the firmware operation directory."));
    return {};
}
} // namespace Firmware
