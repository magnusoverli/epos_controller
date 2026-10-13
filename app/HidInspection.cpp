#include "HidInspection.h"
#include "SysfsReader.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <unistd.h>

std::expected<QVector<HidInspection::Entry>, ScanIssue> HidInspection::enumerate(const QString &hidRoot) {
    auto names = SysfsReader::directories(hidRoot);
    if (!names)
        return std::unexpected(names.error());
    QVector<Entry> entries;
    for (const auto &name : *names) {
        if (!name.startsWith("hidraw"))
            continue;
        const auto path = QFileInfo(QDir(hidRoot).filePath(name + "/device")).canonicalFilePath();
        if (!path.isEmpty())
            entries.append({name, path});
    }
    return entries;
}

QVector<HidInterface> HidInspection::scan(const QString &usbPath, const QString &connectionId,
                                          const QVector<Entry> &entries, const QString &devRoot, bool force) {
    QVector<HidInterface> interfaces;
    const auto usb = QFileInfo(usbPath).canonicalFilePath();
    if (usb.isEmpty())
        return interfaces;
    for (const auto &entry : entries) {
        if (!entry.path.startsWith(usb + '/'))
            continue;
        const auto descriptor = entry.path + "/report_descriptor";
        const auto identity = SysfsReader::identity(descriptor);
        auto cached = m_cache.constFind(entry.path);
        HidInterface info;
        if (!force && cached != m_cache.cend() && cached->connection == connectionId &&
            cached->identity == identity) {
            info = cached->info;
        } else {
            auto bytes =
                SysfsReader::read(descriptor, static_cast<qsizetype>(HidDescriptor::MaxDescriptorBytes));
            if (bytes) {
                info.descriptorBytes = bytes->size();
                info.layout = HidDescriptor::parse(
                    std::as_bytes(std::span(bytes->constData(), static_cast<std::size_t>(bytes->size()))));
            } else {
                info.issues.append(bytes.error());
                info.descriptorTruncated = bytes.error().code == ReadError::TooLarge;
                if (info.descriptorTruncated) {
                    info.descriptorBytes = static_cast<qsizetype>(HidDescriptor::MaxDescriptorBytes + 1);
                    info.layout = std::unexpected(HidDescriptor::ParseError{
                        HidDescriptor::ErrorCode::TooLarge, HidDescriptor::MaxDescriptorBytes});
                }
            }
            auto path = entry.path;
            while (path.startsWith(usb + '/')) {
                const auto numberPath = path + "/bInterfaceNumber";
                auto number = SysfsReader::text(numberPath, 16);
                if (number) {
                    info.interfaceNumber = *number;
                    break;
                }
                if (number.error().code != ReadError::Missing) {
                    info.issues.append(number.error());
                    break;
                }
                path = QFileInfo(path).dir().absolutePath();
            }
            if (info.interfaceNumber.isEmpty() && info.issues.isEmpty())
                info.issues.append(
                    {ReadError::Missing, entry.path, QStringLiteral("USB interface number unavailable")});
            // Failed reads are retried, not cached indefinitely.
            if (info.issues.isEmpty())
                m_cache.insert(entry.path, {connectionId, identity, info});
            else
                m_cache.remove(entry.path);
        }
        info.node = QDir(devRoot).filePath(entry.name);
        const auto native = QFile::encodeName(info.node);
        info.canRead = ::access(native.constData(), R_OK) == 0;
        info.canWrite = ::access(native.constData(), W_OK) == 0;
        interfaces.append(std::move(info));
    }
    // Also discard interfaces removed while the USB device remains present.
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it->connection == connectionId &&
            std::none_of(entries.begin(), entries.end(),
                         [&](const Entry &entry) { return entry.path == it.key(); }))
            it = m_cache.erase(it);
        else
            ++it;
    }
    return interfaces;
}

void HidInspection::prune(const QStringList &connections) {
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (!connections.contains(it->connection))
            it = m_cache.erase(it);
        else
            ++it;
    }
}
