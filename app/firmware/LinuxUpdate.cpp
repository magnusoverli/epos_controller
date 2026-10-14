#include "LinuxUpdate.h"
#include "SysfsDeviceScanner.h"
#include "SysfsReader.h"
#include "UpdateJournal.h"
#include "settings/LinuxHidTransport.h"
#include "settings/SettingsSession.h"
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>
#include <fcntl.h>
#include <thread>
#include <unistd.h>

namespace Firmware {
namespace {
QString errorText(const Settings::Error &error) {
    return QString::fromStdString(error.message);
}
template <typename T> Result<T> convert(Settings::Result<T> result) {
    if (!result)
        return std::unexpected(errorText(result.error()));
    return std::move(*result);
}
Result<void> convert(Settings::Result<void> result) {
    if (!result)
        return std::unexpected(errorText(result.error()));
    return {};
}
std::span<const std::uint8_t> bytes(const QByteArray &data) {
    return {reinterpret_cast<const std::uint8_t *>(data.constData()), static_cast<std::size_t>(data.size())};
}
Result<QString> serial(const DeviceSnapshot &device) {
    auto value = SysfsReader::text(QStringLiteral("/sys/bus/usb/devices/") + device.key + "/serial", 256);
    if (!value || value->isEmpty())
        return std::unexpected(
            QStringLiteral("A stable USB serial is required to identify the headset after reboot."));
    return *value;
}
Result<void> noOtherHandles(const DeviceSnapshot &device) {
    QStringList nodes;
    for (const auto &interface : device.hidInterfaces)
        nodes.append(interface.node);
    // Advisory flock does not constrain Chromium. Also refuse existing visible
    // hidraw users; this is not OS-enforced exclusion of noncooperating software.
    const auto self = QString::number(::getpid());
    for (const auto &pid : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        pid.toUInt(&numeric);
        if (!numeric || pid == self)
            continue;
        const QDir fds("/proc/" + pid + "/fd");
        for (const auto &fd : fds.entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot))
            if (nodes.contains(fd.symLinkTarget()))
                return std::unexpected(
                    QStringLiteral("Another process has the headset open. Close EPOS Connect Web and other "
                                   "headset controllers before installing."));
    }
    return {};
}
class Transport final : public UpdateTransport {
  public:
    Transport(DeviceSnapshot snapshot, QString serial, std::stop_token token)
        : device(std::move(snapshot)), unitSerial(std::move(serial)), stop(token) {}
    Result<void> open() {
        if (auto handles = noOtherHandles(device); !handles)
            return handles;
        auto settings = Settings::openHidTransport(device, stop);
        if (!settings)
            return std::unexpected(errorText(settings.error()));
        settingsIo = std::move(*settings);
        auto update = Settings::openUpdateHidTransport(device, stop);
        if (!update) {
            settingsIo.reset();
            return std::unexpected(errorText(update.error()));
        }
        updateIo = std::move(*update);
        auto current = serial(device);
        if (!current || *current != unitSerial)
            return std::unexpected(
                QStringLiteral("Headset identity changed before opening the update session."));
        return {};
    }
    Result<void> feature(const QByteArray &report) override {
        return convert(updateIo->feature(bytes(report), stop));
    }
    Result<void> output(const QByteArray &report) override {
        return convert(updateIo->send(bytes(report), stop));
    }
    Result<QByteArray> input(UpdateDeadline deadline, std::stop_token token) override {
        auto reply = updateIo->receive(deadline, token);
        if (!reply)
            return std::unexpected(errorText(reply.error()));
        return QByteArray(reinterpret_cast<const char *>(reply->data()), qsizetype(reply->size()));
    }
    Result<void> reconnect(UpdateDeadline deadline, std::stop_token token) override {
        QString oldIdentity;
        for (const auto &interface : device.hidInterfaces)
            if (interface.interfaceNumber == "04")
                oldIdentity = interface.sysfsIdentity;
        settingsIo.reset();
        updateIo.reset();
        SysfsDeviceScanner scanner;
        while (std::chrono::steady_clock::now() < deadline && !token.stop_requested()) {
            auto scan = scanner(true);
            for (const auto &candidate : scan.devices) {
                if (candidate.key != device.key || candidate.usbId != device.usbId)
                    continue;
                auto identity = serial(candidate);
                if (!identity)
                    continue; // Enumeration or permission setup may be incomplete.
                if (*identity != unitSerial)
                    return std::unexpected(
                        QStringLiteral("A different headset appeared on the USB port. Commit was not sent."));
                for (const auto &interface : candidate.hidInterfaces) {
                    if (interface.interfaceNumber != "04" || interface.sysfsIdentity == oldIdentity ||
                        !interface.canRead || !interface.canWrite)
                        continue;
                    device = candidate;
                    return open(); // Fresh handles, descriptor checks and locks; never use a cached fd.
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
        return std::unexpected(QStringLiteral("Headset did not return with the same USB serial before the "
                                              "reconnect deadline. Do not restart the transfer."));
    }
    Result<Version> readVersion() override {
        Settings::Session session(*settingsIo, stop);
        return convert(session.readFirmware());
    }
    Result<void> preflight(Version target) { return verifyUpdatePreflight(*settingsIo, target, stop); }

  private:
    DeviceSnapshot device;
    QString unitSerial;
    std::stop_token stop;
    std::unique_ptr<Settings::ReportTransport> settingsIo;
    std::unique_ptr<Settings::ReportTransport> updateIo;
};
} // namespace
Result<void> verifyUpdatePreflight(Settings::ReportTransport &transport, Version target,
                                   std::stop_token stop) {
    auto compatibility = QByteArray::fromHex("01ebfe0100000001000c000000190012000000000000000200");
    qToLittleEndian<quint16>(target.major, compatibility.data() + 17);
    qToLittleEndian<quint16>(target.minor, compatibility.data() + 19);
    qToLittleEndian<quint16>(target.patch, compatibility.data() + 21);
    compatibility[11] = char(EposProtocol::checksum(bytes(compatibility).subspan(13, 12)));
    const std::array<QByteArray, 2> requests{QByteArray::fromHex("01ebfe0100000001000400050019000100"),
                                             compatibility};
    for (auto request : requests) {
        if (stop.stop_requested())
            return std::unexpected(QStringLiteral("Firmware preflight cancelled."));
        request.resize(107, '\0');
        if (auto sent = transport.send(bytes(request), stop); !sent)
            return std::unexpected(errorText(sent.error()));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{2};
        bool matched = false;
        for (int ignored = 0; ignored < 128 && std::chrono::steady_clock::now() < deadline; ++ignored) {
            auto incoming = transport.receive(deadline, stop);
            if (!incoming)
                return std::unexpected(errorText(incoming.error()));
            if (incoming->empty() || (*incoming)[0] != 1)
                continue;
            const QByteArray reply(reinterpret_cast<const char *>(incoming->data()),
                                   qsizetype(incoming->size()));
            if (reply.size() >= 17 && reply.mid(13, 4) != request.mid(13, 4))
                continue;
            if (reply.size() != 107 || reply.left(6) != QByteArray::fromHex("01ebfe010000") ||
                reply[7] != 0 || reply[8] != 0 || reply[10] != 0 || reply[12] != 0)
                return std::unexpected(QStringLiteral("Invalid firmware preflight reply header."));
            const auto length = quint8(reply[9]);
            if (length < 4 || length > 94 || length % 2 != 0 ||
                EposProtocol::checksum(bytes(reply).subspan(13, length)) != quint8(reply[11]) ||
                !std::all_of(reply.cbegin() + 13 + length, reply.cend(), [](char b) { return b == 0; }))
                return std::unexpected(
                    QStringLiteral("Invalid firmware preflight reply length or checksum."));
            const auto status = quint8(reply[6]);
            if (status != 0 && status != 0x10 && status != 0x20)
                return std::unexpected(QStringLiteral("Headset rejected firmware preflight (status 0x%1).")
                                           .arg(status, 2, 16, QLatin1Char('0')));
            // CommandUnknown/CommandNotSupported are the vendor's legacy fallback.
            // A supported compatibility command supplies status + alternative method.
            if (status == 0) {
                if (request[15] == 1) {
                    if (length != 6)
                        return std::unexpected(QStringLiteral("Invalid firmware capability reply."));
                } else {
                    if (length != 8)
                        return std::unexpected(QStringLiteral("Invalid firmware compatibility reply."));
                    const auto compatible = qFromLittleEndian<quint16>(incoming->data() + 17);
                    const auto method = qFromLittleEndian<quint16>(incoming->data() + 19);
                    if (compatible == 1)
                        return std::unexpected(
                            QStringLiteral("The headset reports that this firmware is incompatible."));
                    if (compatible != 0 && !(compatible == 2 && method == 4))
                        return std::unexpected(
                            QStringLiteral("The headset requires a different firmware update method."));
                }
            }
            matched = true;
            break;
        }
        if (!matched)
            return std::unexpected(
                QStringLiteral("No matching firmware preflight reply. Firmware transfer was not started."));
    }
    return {};
}
UpdateOutcome installLinuxUpdate(const DeviceSnapshot &device, const QByteArray &archive,
                                 const Release &release, Version installed, const UpdateProgress &progress,
                                 std::stop_token stop) {
    auto failure = [](const QString &error) { return UpdateOutcome{false, false, error}; };
    auto prepared = prepareUpdate(archive, release, installed);
    if (!prepared)
        return failure(prepared.error());
    if (device.usbId != "1395:018e" || stop.stop_requested())
        return failure(QStringLiteral("Unsupported headset or cancelled installation."));
    auto unitSerial = serial(device);
    if (!unitSerial)
        return failure(unitSerial.error());
    const auto hash = QCryptographicHash::hash(unitSerial->toUtf8(), QCryptographicHash::Sha256).toHex();
    const auto directory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/firmware";
    UpdateJournal journal(directory, hash);
    if (auto acquired = journal.acquire(); !acquired)
        return failure(acquired.error());
    Transport io(device, *unitSerial, stop);
    if (auto opened = io.open(); !opened)
        return failure(opened.error());
    auto actual = io.readVersion();
    if (!actual)
        return failure(actual.error());
    if (*actual != installed || !supportedUpgrade(*actual, release))
        return failure(
            QStringLiteral("Installed firmware changed. Refresh the release information and confirm again."));
    if (auto preflight = io.preflight(release.version); !preflight)
        return failure(preflight.error());
    QDBusInterface login(QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
                         QStringLiteral("org.freedesktop.login1.Manager"), QDBusConnection::systemBus());
    const QDBusReply<QDBusUnixFileDescriptor> inhibition = login.call(
        QStringLiteral("Inhibit"), QStringLiteral("sleep:shutdown"), QStringLiteral("Headset Controller"),
        QStringLiteral("Installing headset firmware"), QStringLiteral("block"));
    if (!inhibition.isValid() || !inhibition.value().isValid())
        return failure(QStringLiteral("Cannot inhibit system sleep/shutdown during firmware installation: %1")
                           .arg(inhibition.error().message()));
    QJsonObject record{{"schemaVersion", 1},
                       {"state", "in-progress"},
                       {"unitHash", QString::fromLatin1(hash)},
                       {"port", device.key},
                       {"source", QString::fromStdString(installed.text())},
                       {"target", QString::fromStdString(release.version.text())},
                       {"packageSha256", QString::fromLatin1(release.archiveHash)}};
    std::optional<UpdateStage> savedStage;
    const UpdateProgress track = [&](UpdateStage stage, qsizetype done, qsizetype total) -> Result<void> {
        if (!savedStage || *savedStage != stage) {
            record["phase"] = updateStageName(stage);
            record["bytesSent"] = qint64(done);
            record["updatedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            if (auto written = journal.write(record); !written)
                return written;
            savedStage = stage;
        }
        return progress(stage, done, total);
    };
    auto outcome = runApplicationUpdate(io, prepared->image, prepared->version, track, stop);
    record["state"] = outcome.verified               ? "verified"
                      : outcome.deviceMayHaveChanged ? "interrupted"
                                                     : "not-started";
    record["error"] = outcome.error;
    if (auto written = journal.write(record); !written) {
        outcome.verified = false;
        outcome.error += '\n' + written.error();
    }
    if (!outcome.verified && outcome.deviceMayHaveChanged)
        outcome.error += QStringLiteral("\nUpdate state is uncertain. Native retry is blocked; use EPOS "
                                        "recovery/support. Operation record: %1")
                             .arg(journal.path());
    return outcome;
}
} // namespace Firmware
