#include "FirmwareUpdateFixture.h"
#include "firmware/ApplicationUpdate.h"
#include "firmware/LinuxUpdate.h"
#include "firmware/ReleaseCatalog.h"
#include "firmware/UpdateJournal.h"
#include "settings/EposProtocol.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

namespace {
using namespace Firmware;
const Version Target{1, 5, 619};
const UpdateProgress IgnoreProgress = [](UpdateStage, qsizetype, qsizetype) -> Result<void> { return {}; };
struct Event {
    char kind;
    QByteArray bytes;
};
class Replay final : public UpdateTransport {
  public:
    QList<Event> events;
    qsizetype position = 0;
    qsizetype failAt = -1;
    Version version = Target;
    Result<QByteArray> next(char kind, const QByteArray &bytes = {}) {
        if (position >= events.size())
            return std::unexpected(QStringLiteral("Replay exhausted"));
        const auto event = events[position++];
        if (position - 1 == failAt)
            return std::unexpected(QStringLiteral("Injected transport failure"));
        if (event.kind != kind || (kind != 'i' && event.bytes != bytes))
            return std::unexpected(QStringLiteral("Wire replay mismatch at event %1").arg(position - 1));
        return event.bytes;
    }
    Result<void> perform(char kind, const QByteArray &bytes = {}) {
        auto result = next(kind, bytes);
        if (!result)
            return std::unexpected(result.error());
        return {};
    }
    Result<void> feature(const QByteArray &b) override { return perform('f', b); }
    Result<void> output(const QByteArray &b) override { return perform('o', b); }
    Result<QByteArray> input(UpdateDeadline, std::stop_token) override { return next('i'); }
    Result<void> reconnect(UpdateDeadline, std::stop_token) override { return perform('r'); }
    Result<Version> readVersion() override {
        auto read = perform('v');
        if (!read)
            return std::unexpected(read.error());
        return version;
    }
    void add(char kind, const char *hex) {
        auto data = QByteArray::fromHex(hex);
        if (kind == 'o')
            data.resize(255, '\0');
        events.append({kind, data});
    }
};
// Independently transcribed control reports from the successful USB trace.
// Generated data is synthetic; request run lengths below preserve all 9,090
// captured byte requests. The optional full-wire replay uses the actual capture.
Replay transcript(const QByteArray &image, const QList<std::pair<int, int>> &runs) {
    Replay io;
    io.add('f', "03010200");
    io.add('i', "06010000000000000000000000");
    io.add('o', "0503070000");
    io.add('i', "06030800000000000000000000");
    io.add('o', "050713000401020304");
    io.add('i', "06091400060001020304050000");
    io.add('o', "0503010000");
    io.add('i', "06060200030006660000000000");
    io.add('o', "0503150000");
    qsizetype cursor = 0;
    for (const auto &[count, repetitions] : runs) {
        for (int repeat = 0; repeat < repetitions; ++repeat) {
            auto request = QByteArray::fromHex("060b0300080000000000000000");
            qToBigEndian<quint32>(quint32(count), request.data() + 5);
            io.events.append({'i', request});
            for (int left = count; left > 0;) {
                const int n = std::min(249, left);
                auto output = QByteArray::fromHex("050004000000");
                output[1] = char(n + 4);
                qToBigEndian<quint16>(quint16(n + 1), output.data() + 3);
                output[5] = cursor + n == image.size() ? 1 : 0;
                output += image.mid(cursor, n);
                output.resize(255, '\0');
                io.events.append({'o', output});
                cursor += n;
                left -= n;
            }
        }
    }
    io.add('o', "0503160000");
    io.add('i', "06030b00000000000000000000");
    io.add('o', "05040c000100");
    io.add('r', "");
    io.add('f', "03010200");
    io.add('i', "06010000000000000000000000");
    io.add('o', "050713000401020304");
    io.add('i', "06091400060301020304050000");
    io.add('o', "0503010000");
    io.add('i', "06060200030006660000000000");
    io.add('o', "05040e000100");
    io.add('i', "06030f00000000000000000000");
    io.add('o', "050410000100");
    io.add('i', "06031200000000000000000000");
    io.add('f', "03010700");
    io.add('v', "");
    return io;
}
} // namespace
class FirmwareUpdateTest final : public QObject {
    Q_OBJECT
  private slots:
    void capturedRequestSequence() {
        QByteArray image(6774556, '\0');
        for (qsizetype i = 0; i < image.size(); ++i)
            image[i] = char(i % 251);
        auto io = transcript(image,
                             {{12, 1}, {36, 1},    {12, 1},  {8, 1},  {12, 2}, {8, 1},      {524, 1}, {12, 1},
                              {8, 1},  {747, 3},   {555, 1}, {12, 1}, {8, 1},  {747, 781},  {157, 1}, {12, 1},
                              {8, 1},  {747, 87},  {271, 1}, {12, 1}, {8, 1},  {747, 7265}, {545, 1}, {12, 1},
                              {8, 1},  {747, 929}, {481, 1}, {12, 1}, {256, 1}});
        auto result = runApplicationUpdate(io, image, Target, IgnoreProgress);
        QVERIFY2(result.verified, qPrintable(result.error));
        QCOMPARE(io.position, io.events.size());
        QCOMPARE(std::count_if(io.events.begin(), io.events.end(),
                               [](const auto &e) { return e.kind == 'o' && e.bytes[2] == 4; }),
                 27229);
    }
    void everyTransportFailureStopsWithoutRetry() {
        const QByteArray image(536, 's');
        const auto reference = transcript(image, {{12, 1}, {524, 1}});
        for (qsizetype fail = 0; fail < reference.events.size(); ++fail) {
            auto io = reference;
            io.failAt = fail;
            const auto result = runApplicationUpdate(io, image, Target, IgnoreProgress);
            QVERIFY(!result.verified);
            QVERIFY(result.deviceMayHaveChanged);
            QCOMPARE(io.position, fail + 1);
        }
    }
    void malformedAndUnvalidatedResponses_data() {
        QTest::addColumn<int>("index");
        QTest::addColumn<QByteArray>("replacement");
        const auto add = [](const char *name, int i, const char *hex) {
            QTest::newRow(name) << i << QByteArray::fromHex(hex);
        };
        add("wrong-connect", 1, "06010100000000000000000000");
        add("wrong-resume", 5, "06091400060101020304050000");
        add("wrong-cookie", 5, "06091400060001020305050000");
        add("nonzero-offset", 9, "060b0300080000000c00000001");
        add("zero-request", 9, "060b0300080000000000000000");
        add("oversized-request", 9, "060b0300087fffffff00000000");
        add("truncated-request", 9, "060b0300080000000c000000");
        add("wrong-report-id", 9, "050b0300080000000c00000000");
        add("validation-pending", 16, "06051700020064000000000000");
        add("device-error", 16, "06051100021234000000000000");
        add("invalid-padding", 16, "06030b00000000000000000001");
    }
    void malformedAndUnvalidatedResponses() {
        QFETCH(int, index);
        QFETCH(QByteArray, replacement);
        const QByteArray image(536, 's');
        auto io = transcript(image, {{12, 1}, {524, 1}});
        QCOMPARE(io.events[index].kind, 'i');
        io.events[index].bytes = replacement;
        auto result = runApplicationUpdate(io, image, Target, IgnoreProgress);
        QVERIFY(!result.verified);
        QCOMPARE(io.position, index + 1);
    }
    void versionMismatchCancellationAndJournalFailure() {
        const QByteArray image(536, 's');
        auto io = transcript(image, {{12, 1}, {524, 1}});
        io.version = {1, 5, 64};
        QVERIFY(!runApplicationUpdate(io, image, Target, IgnoreProgress).verified);
        io.position = 0;
        std::stop_source stop;
        stop.request_stop();
        auto cancelled = runApplicationUpdate(io, image, Target, IgnoreProgress, stop.get_token());
        QVERIFY(!cancelled.verified);
        QVERIFY(!cancelled.deviceMayHaveChanged);
        QCOMPARE(io.position, 0);
        auto blocked =
            runApplicationUpdate(io, image, Target, [](UpdateStage, qsizetype, qsizetype) -> Result<void> {
                return std::unexpected(QStringLiteral("Disk full"));
            });
        QVERIFY(!blocked.verified);
        QVERIFY(!blocked.deviceMayHaveChanged);
        QCOMPARE(io.position, 0);
    }
    void durableInterruptionAndConcurrentLock() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QByteArray hash(64, 'a');
        {
            UpdateJournal journal(directory.path(), hash);
            QVERIFY(journal.acquire());
            UpdateJournal competing(directory.path(), hash);
            QVERIFY(!competing.acquire());
            QVERIFY(
                journal.write({{"schemaVersion", 1}, {"state", "in-progress"}, {"phase", "transferring"}}));
        } // Simulate losing the process without a final success record.
        {
            UpdateJournal restarted(directory.path(), hash);
            QVERIFY(!restarted.acquire());
        }
        UpdateJournal differentUnit(directory.path(), QByteArray(64, 'b'));
        QVERIFY(differentUnit.acquire());
        QVERIFY(differentUnit.write({{"schemaVersion", 1}, {"state", "verified"}}));
    }
    void releasePolicyRequiresNewerModelRelease() {
        Release release{Target,
                        QUrl("https://update.eposstorage.com/deviceupdates/IMPACT_860T_ANC_1.5.619.zip"),
                        "1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd"};
        QVERIFY(supportedUpgrade({1, 5, 64}, release));
        QVERIFY(!supportedUpgrade(Target, release));
        QVERIFY(supportedUpgrade({1, 4, 1}, release));
        QVERIFY(!supportedUpgrade({1, 6, 0}, release));
        QVERIFY(!prepareUpdate("not a package", release, {1, 5, 64}));
        release.archiveHash[0] = 'z';
        QVERIFY(!supportedUpgrade({1, 5, 64}, release));
    }
    void futurePackagesUseCatalogMetadataAndSameTransfer() {
        for (const auto version : {Version{1, 6, 0}, Version{2, 0, 0}}) {
            const auto zip = SyntheticUpdate::zip(version);
            QVERIFY(!zip.isEmpty());
            const auto release = SyntheticUpdate::release(version, zip);
            QVERIFY(supportedUpgrade(Target, release));
            auto prepared = prepareUpdate(zip, release, Target);
            QVERIFY2(prepared.has_value(), qPrintable(prepared ? QString() : prepared.error()));
            QCOMPARE(prepared->version, version);
            QCOMPARE(prepared->image, SyntheticUpdate::image());
            auto io = transcript(prepared->image, {{12, 1}, {int(prepared->image.size()) - 12, 1}});
            io.version = version;
            const auto outcome = runApplicationUpdate(io, prepared->image, version, IgnoreProgress);
            QVERIFY2(outcome.verified, qPrintable(outcome.error));
            QCOMPARE(io.position, io.events.size());
            auto mismatched = release;
            mismatched.archiveHash[0] = mismatched.archiveHash[0] == '0' ? '1' : '0';
            QVERIFY(!prepareUpdate(zip, mismatched, Target));
            mismatched = release;
            mismatched.version.patch++;
            mismatched.url = SyntheticUpdate::release(mismatched.version, zip).url;
            QVERIFY(!prepareUpdate(zip, mismatched, Target)); // Valid hash, wrong manifest version.
            mismatched = release;
            mismatched.url = QUrl("https://update.eposstorage.com/deviceupdates/OTHER_MODEL_1.6.0.zip");
            QVERIFY(!supportedUpgrade(Target, mismatched));
            QVERIFY(!prepareUpdate(zip, release, version));
            const auto otherModel = SyntheticUpdate::zip(version, "OTHER MODEL");
            QVERIFY(!prepareUpdate(otherModel, SyntheticUpdate::release(version, otherModel), Target));
            const auto badImage = SyntheticUpdate::zip(version, "IMPACT 860T ANC", "invalid image");
            QVERIFY(!prepareUpdate(badImage, SyntheticUpdate::release(version, badImage), Target));
        }
    }
    void targetVersionPreflightAndLegacyFallback() {
        class Legacy final : public Settings::ReportTransport {
          public:
            int sent = 0;
            int corrupt = -1;
            quint8 commandStatus = 0x10;
            quint16 compatibility = 0;
            quint16 method = 0;
            QList<QByteArray> requests;
            Settings::Result<void> send(std::span<const std::uint8_t> report, std::stop_token) override {
                if (report.size() != 107)
                    return std::unexpected(
                        Settings::Error{Settings::ErrorCode::Io, "Wrong preflight report length"});
                ++sent;
                requests.append(
                    QByteArray(reinterpret_cast<const char *>(report.data()), qsizetype(report.size())));
                return {};
            }
            Settings::Result<std::vector<std::uint8_t>> receive(Settings::Deadline,
                                                                std::stop_token) override {
                auto reply = sent == 1 ? QByteArray::fromHex("01ebfe01000000000006004100190001000000")
                                       : requests.last();
                reply.resize(107, '\0');
                if (sent == 2) {
                    reply[7] = 0;
                    reply[6] = char(commandStatus);
                    if (commandStatus == 0) {
                        reply[9] = 8;
                        qToLittleEndian<quint16>(compatibility, reply.data() + 17);
                        qToLittleEndian<quint16>(method, reply.data() + 19);
                        std::fill(reply.begin() + 21, reply.end(), '\0');
                    }
                    reply[11] = char(EposProtocol::checksum(std::span(
                        reinterpret_cast<const std::uint8_t *>(reply.constData()) + 13, quint8(reply[9]))));
                }
                if (corrupt >= 0)
                    reply[corrupt] = char(quint8(reply[corrupt]) ^ 1);
                return std::vector<std::uint8_t>(reply.begin(), reply.end());
            }
        };
        Legacy valid;
        QVERIFY(verifyUpdatePreflight(valid, Target, {}));
        QCOMPARE(valid.sent, 2);
        QCOMPARE(valid.requests[1].left(25).toHex(),
                 QByteArray("01ebfe0100000001000c00c80019001200010005006b020200"));
        for (const auto status : {0, 0x10, 0x20}) {
            Legacy future;
            future.commandStatus = quint8(status);
            QVERIFY(verifyUpdatePreflight(future, {2, 3, 4567}, {}));
            QCOMPARE(future.requests[1].mid(17, 6).toHex(), QByteArray("02000300d711"));
        }
        Legacy alternative;
        alternative.commandStatus = 0;
        alternative.compatibility = 2;
        alternative.method = 4;
        QVERIFY(verifyUpdatePreflight(alternative, {1, 6, 0}, {}));
        for (const auto status : {1, 2, 3}) {
            Legacy rejected;
            rejected.commandStatus = 0;
            rejected.compatibility = quint16(status);
            QVERIFY(!verifyUpdatePreflight(rejected, {1, 6, 0}, {}));
        }
        Legacy error;
        error.commandStatus = 0x30;
        QVERIFY(!verifyUpdatePreflight(error, {1, 6, 0}, {}));
        for (int byte : {1, 6, 9, 11, 17, 106}) {
            Legacy invalid;
            invalid.corrupt = byte;
            QVERIFY(!verifyUpdatePreflight(invalid, Target, {}));
            QCOMPARE(invalid.sent, 1);
        }
    }
    void optionalOfficialPackage() {
        const auto path = qEnvironmentVariable("HEADSET_UPDATE_ARCHIVE");
        if (path.isEmpty())
            QSKIP("Set HEADSET_UPDATE_ARCHIVE to the preserved official ZIP for pinned package inspection.");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto zip = file.readAll();
        Release release{Target,
                        QUrl("https://update.eposstorage.com/deviceupdates/IMPACT_860T_ANC_1.5.619.zip"),
                        "1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd"};
        const auto result = prepareUpdate(zip, release, {1, 5, 64});
        QVERIFY2(result.has_value(), qPrintable(result ? QString() : result.error()));
        QCOMPARE(result->image.size(), 6774556);
        auto corrupt = zip;
        corrupt[corrupt.size() / 2] ^= 1;
        QVERIFY(!prepareUpdate(corrupt, release, {1, 5, 64}));
        QVERIFY(!prepareUpdate(zip, release, Target));
    }
    void optionalFullWireReplay() {
        const auto directory = qEnvironmentVariable("HEADSET_UPDATE_REPLAY");
        if (directory.isEmpty())
            QSKIP("Set HEADSET_UPDATE_REPLAY to the preserved successful capture directory for full-wire "
                  "replay.");
        QFile input(directory + "/decoded-update.json"), imageFile(directory + "/transferred-image.bin");
        QVERIFY(input.open(QIODevice::ReadOnly));
        QVERIFY(imageFile.open(QIODevice::ReadOnly));
        const auto image = imageFile.readAll();
        QCOMPARE(QCryptographicHash::hash(image, QCryptographicHash::Sha256).toHex(),
                 QByteArray("ab0b74926fef1b3bfbcdb3f92b0270ef7f730fe2bc0226914082c7286af43978"));
        Replay io;
        int previousDevice = 28;
        for (const auto &value : QJsonDocument::fromJson(input.readAll()).object()["reports"].toArray()) {
            const auto row = value.toObject();
            const auto data = QByteArray::fromHex(row["data"].toString().toLatin1());
            const auto device = row["device"].toInt();
            if (device != previousDevice) {
                io.add('r', "");
                previousDevice = device;
            }
            if (row["event"] == "S") {
                QCOMPARE(row["completionStatus"].toInt(-1), 0);
                io.events.append({data[0] == 3 ? 'f' : 'o', data});
            } else
                io.events.append({'i', data});
        }
        io.add('v', "");
        const auto result = runApplicationUpdate(io, image, Target, IgnoreProgress);
        QVERIFY2(result.verified, qPrintable(result.error));
        QCOMPARE(io.position, io.events.size());
    }
};
QTEST_GUILESS_MAIN(FirmwareUpdateTest)
#include "firmware-update_test.moc"
