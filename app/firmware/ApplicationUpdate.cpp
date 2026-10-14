#include "ApplicationUpdate.h"
#include <QtEndian>
#include <algorithm>

namespace Firmware {
namespace {
using Clock = std::chrono::steady_clock;
struct Message {
    quint8 opcode;
    QByteArray payload;
};
Result<Message> decode(const QByteArray &bytes) {
    if (bytes.size() < 5 || bytes.size() > 66 || quint8(bytes[0]) != 6)
        return std::unexpected(QStringLiteral("Invalid update input report."));
    const auto size = qFromBigEndian<quint16>(bytes.constData() + 3);
    if (size > 61 || bytes.size() < 5 + size || quint8(bytes[1]) != size + 3 ||
        !std::all_of(bytes.cbegin() + 5 + size, bytes.cend(), [](char b) { return b == 0; }))
        return std::unexpected(QStringLiteral("Invalid update response length or padding."));
    return Message{quint8(bytes[2]), bytes.mid(5, size)};
}
QByteArray outputReport(quint8 opcode, const QByteArray &payload) {
    QByteArray report(255, '\0');
    report[0] = 5;
    report[1] = char(payload.size() + 3);
    report[2] = char(opcode);
    qToBigEndian<quint16>(quint16(payload.size()), report.data() + 3);
    std::copy(payload.cbegin(), payload.cend(), report.begin() + 5);
    return report;
}

class Session {
  public:
    Session(UpdateTransport &transport, const QByteArray &data, Version version,
            const UpdateProgress &callback, std::stop_token token)
        : io(transport), image(data), target(version), progress(callback), stop(token) {}
    UpdateOutcome run() {
        auto result = perform();
        return {bool(result), changed, result ? QString() : result.error()};
    }

  private:
    Result<void> checkpoint() const {
        if (stop.stop_requested())
            return std::unexpected(QStringLiteral("Update interrupted; do not automatically retry."));
        if (Clock::now() >= overall)
            return std::unexpected(QStringLiteral("Firmware update exceeded its overall deadline."));
        return {};
    }
    Result<void> stage(UpdateStage value) {
        if (auto check = checkpoint(); !check)
            return check;
        return progress(value, cursor, image.size());
    }
    Result<void> send(quint8 opcode, const QByteArray &payload = {}) {
        if (auto check = checkpoint(); !check)
            return check;
        if (payload.size() > 250)
            return std::unexpected(QStringLiteral("Firmware output payload is too large."));
        changed = true; // A failed ioctl can still have reached the device.
        return io.output(outputReport(opcode, payload));
    }
    Result<Message> receive(std::chrono::seconds timeout) {
        if (auto check = checkpoint(); !check)
            return std::unexpected(check.error());
        auto bytes = io.input(std::min(overall, Clock::now() + timeout), stop);
        if (!bytes)
            return std::unexpected(bytes.error());
        auto reply = decode(*bytes);
        if (reply && reply->opcode == 0x11)
            return std::unexpected(
                QStringLiteral("Headset rejected the update (error payload %1). No retry was sent.")
                    .arg(QString::fromLatin1(reply->payload.toHex())));
        return reply;
    }
    Result<QByteArray> expect(quint8 opcode, qsizetype size,
                              std::chrono::seconds timeout = std::chrono::seconds{30}) {
        auto reply = receive(timeout);
        if (!reply)
            return std::unexpected(reply.error());
        if (reply->opcode != opcode || reply->payload.size() != size)
            return std::unexpected(
                QStringLiteral("Unexpected update response 0x%1; stopped without advancing.")
                    .arg(reply->opcode, 2, 16, QLatin1Char('0')));
        return reply->payload;
    }
    Result<void> negotiate(bool postboot) {
        if (auto check = checkpoint(); !check)
            return check;
        changed = true;
        if (auto sent = io.feature(QByteArray::fromHex("03010200")); !sent)
            return sent;
        auto connected = io.input(std::min(overall, Clock::now() + std::chrono::seconds{30}), stop);
        if (!connected)
            return std::unexpected(connected.error());
        if (connected->size() < 3 || connected->size() > 66 ||
            connected->left(3) != QByteArray::fromHex("060100") ||
            !std::all_of(connected->cbegin() + 3, connected->cend(), [](char b) { return b == 0; }))
            return std::unexpected(
                QStringLiteral("Headset did not accept the update connection. No reset or retry was sent."));
        if (!postboot) {
            if (auto sent = send(7); !sent)
                return sent;
            if (auto ack = expect(8, 0); !ack)
                return std::unexpected(ack.error());
        }
        if (auto sent = send(0x13, QByteArray::fromHex("01020304")); !sent)
            return sent;
        auto sync = expect(0x14, 6);
        if (!sync)
            return std::unexpected(sync.error());
        if (quint8((*sync)[0]) != (postboot ? 3 : 0) || sync->mid(1, 4) != QByteArray::fromHex("01020304") ||
            quint8((*sync)[5]) != 5)
            return std::unexpected(
                QStringLiteral("Unexpected update resume point, file ID or sync extension."));
        if (auto sent = send(1); !sent)
            return sent;
        auto started = expect(2, 3);
        if (!started)
            return std::unexpected(started.error());
        if ((*started)[0] != 0)
            return std::unexpected(QStringLiteral("Headset refused to start the update."));
        return {};
    }
    Result<void> perform() {
        if (image.isEmpty() || image.size() > 32 * 1024 * 1024)
            return std::unexpected(QStringLiteral("Invalid firmware image size."));
        if (auto saved = stage(UpdateStage::Connecting); !saved)
            return saved;
        if (auto connected = negotiate(false); !connected)
            return connected;
        if (auto saved = stage(UpdateStage::Transferring); !saved)
            return saved;
        if (auto sent = send(0x15); !sent)
            return sent;
        while (cursor < image.size()) {
            auto request = expect(3, 8, std::chrono::seconds{100});
            if (!request)
                return std::unexpected(request.error());
            auto count = qFromBigEndian<quint32>(request->constData());
            const auto offset = qFromBigEndian<quint32>(request->constData() + 4);
            // Only zero offsets were observed in all 9,090 successful requests.
            if (offset != 0 || count == 0 || count > quint64(image.size() - cursor))
                return std::unexpected(QStringLiteral(
                    "Unvalidated firmware byte request; transfer stopped without guessing offsets."));
            while (count > 0) {
                const auto size = std::min(quint32(249), count);
                QByteArray payload(1, cursor + size == image.size() ? '\1' : '\0');
                payload += image.mid(cursor, size);
                if (auto sent = send(4, payload); !sent)
                    return sent;
                count -= size;
                cursor += size;
            }
            if (auto saved = stage(UpdateStage::Transferring); !saved)
                return saved;
        }
        if (auto saved = stage(UpdateStage::Validating); !saved)
            return saved;
        if (auto sent = send(0x16); !sent)
            return sent;
        // Pending 0x17 is NOT validation success. Its delay contract is unvalidated.
        if (auto valid = expect(0x0b, 0, std::chrono::seconds{100}); !valid)
            return std::unexpected(valid.error());
        if (auto saved = stage(UpdateStage::Reconnecting); !saved)
            return saved;
        if (auto sent = send(0x0c, QByteArray(1, '\0')); !sent)
            return sent;
        if (auto reconnected = io.reconnect(std::min(overall, Clock::now() + std::chrono::seconds{90}), stop);
            !reconnected)
            return reconnected;
        if (auto connected = negotiate(true); !connected)
            return connected;
        if (auto saved = stage(UpdateStage::Committing); !saved)
            return saved;
        if (auto sent = send(0x0e, QByteArray(1, '\0')); !sent)
            return sent;
        if (auto ack = expect(0x0f, 0); !ack)
            return std::unexpected(ack.error());
        if (auto sent = send(0x10, QByteArray(1, '\0')); !sent)
            return sent;
        if (auto ack = expect(0x12, 0); !ack)
            return std::unexpected(ack.error());
        if (auto ended = io.feature(QByteArray::fromHex("03010700")); !ended)
            return ended;
        if (auto saved = stage(UpdateStage::Verifying); !saved)
            return saved;
        auto installed = io.readVersion();
        if (!installed)
            return std::unexpected(installed.error());
        if (*installed != target)
            return std::unexpected(QStringLiteral("Commit acknowledged, but the installed firmware does not "
                                                  "match the target. Do not retry automatically."));
        return {};
    }
    UpdateTransport &io;
    const QByteArray &image;
    Version target;
    const UpdateProgress &progress;
    std::stop_token stop;
    UpdateDeadline overall = Clock::now() + std::chrono::minutes{15};
    qsizetype cursor = 0;
    bool changed = false;
};
} // namespace
UpdateOutcome runApplicationUpdate(UpdateTransport &transport, const QByteArray &image, Version target,
                                   const UpdateProgress &progress, std::stop_token stop) {
    return Session(transport, image, target, progress, stop).run();
}
QString updateStageName(UpdateStage stage) {
    switch (stage) {
    case UpdateStage::Connecting:
        return QStringLiteral("connecting");
    case UpdateStage::Transferring:
        return QStringLiteral("transferring");
    case UpdateStage::Validating:
        return QStringLiteral("validating");
    case UpdateStage::Reconnecting:
        return QStringLiteral("reconnecting");
    case UpdateStage::Committing:
        return QStringLiteral("committing");
    case UpdateStage::Verifying:
        return QStringLiteral("verifying");
    }
    return QStringLiteral("unknown");
}
} // namespace Firmware
