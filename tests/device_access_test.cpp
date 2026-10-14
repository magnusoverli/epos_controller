#include "DeviceAccessSetup.h"
#include "FakeSettingsTransport.h"
#include "TestFixtures.h"
#include "UdevRule.h"
#include "access/RuleInstaller.h"
#include <QCoreApplication>
#include <QFile>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
Settings::TransportFactory transport() {
    return [hardware = std::make_shared<FakeSettingsDevice>()](
               const DeviceSnapshot &,
               std::stop_token) -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
        return std::make_unique<FakeSettingsTransport>(hardware);
    };
}
DeviceAccessSetup::Options options(const QTemporaryDir &directory, int exitCode) {
    return {.program = QCoreApplication::applicationFilePath(),
            .arguments = {"--fake-setup", QString::number(exitCode), directory.filePath("activated")},
            .stateFile = directory.filePath("state.ini"),
            .settleMilliseconds = 20};
}
DeviceSnapshot lockedDevice() {
    auto device = sampleSettingsDevice();
    device.hidInterfaces[0].canRead = device.hidInterfaces[0].canWrite = false;
    return device;
}
} // namespace

class DeviceAccessTest : public QObject {
    Q_OBJECT
  private slots:
    void permissionsSettleWithoutAuthorization() {
        QTemporaryDir directory;
        std::atomic<int> scans{0};
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult result;
                result.devices = {++scans == 1 ? lockedDevice() : sampleSettingsDevice()};
                return result;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController settings(discovery, transport());
        DeviceAccessSetup setup(discovery, settings, options(directory, 0));
        QTRY_VERIFY(scans >= 2);
        QTRY_VERIFY(!discovery.busy());
        QTRY_VERIFY(!settings.needsAccess());
        QVERIFY(!QFile::exists(directory.filePath("activated")));
        QVERIFY(!QFile::exists(directory.filePath("state.ini")));
    }
    void automaticSetupAndReadback() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult result;
                result.devices = {QFile::exists(directory.filePath("activated")) ? sampleSettingsDevice()
                                                                                 : lockedDevice()};
                return result;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController settings(discovery, transport());
        DeviceAccessSetup setup(discovery, settings, options(directory, 0));
        QSignalSpy installed(&setup, &DeviceAccessSetup::installed);
        // No activation of Device settings and no GUI action are needed.
        QTRY_COMPARE(installed.count(), 1);
        QTRY_VERIFY(settings.ready());
        QVERIFY(!settings.needsAccess());
        QVERIFY(!setup.canRetry());
        QVERIFY(QFile::exists(directory.filePath("activated")));
        QSettings state(directory.filePath("state.ini"), QSettings::IniFormat);
        QVERIFY(state.value("automaticAttempted").toBool());
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(installed.count(), 1);
    }

    void cancellationRememberedAndExplicitRetry() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult result;
                result.devices = {lockedDevice()};
                return result;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController settings(discovery, transport());
        {
            DeviceAccessSetup setup(discovery, settings, options(directory, 126));
            QTRY_VERIFY(setup.message().contains("cancelled"));
            QVERIFY(setup.canRetry());
            QVERIFY(!setup.busy());
            discovery.refresh(true);
            QTRY_VERIFY(!discovery.busy());
            QVERIFY(setup.message().contains("cancelled"));
        }
        // A new application instance must not start a new authorization prompt.
        DeviceAccessSetup next(discovery, settings, options(directory, 0));
        QSignalSpy installed(&next, &DeviceAccessSetup::installed);
        QCoreApplication::processEvents();
        QVERIFY(!next.busy());
        QVERIFY(!QFile::exists(directory.filePath("activated")));
        QVERIFY(next.canRetry());
        next.enableAccess();
        next.enableAccess(); // No second concurrent authorization process.
        QTRY_COMPARE(installed.count(), 1);
        QTRY_VERIFY(settings.ready());
    }

    void noAutomaticPrompt_data() {
        QTest::addColumn<bool>("automatic");
        QTest::addColumn<bool>("locked");
        QTest::addColumn<bool>("supported");
        QTest::newRow("screenshot-or-smoke") << false << true << true;
        QTest::newRow("already-accessible") << true << false << true;
        QTest::newRow("unsupported-headset") << true << true << false;
    }
    void noAutomaticPrompt() {
        QFETCH(bool, automatic);
        QFETCH(bool, locked);
        QFETCH(bool, supported);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DeviceDiscovery discovery(
            [=](bool) {
                ScanResult result;
                auto device = locked ? lockedDevice() : sampleSettingsDevice();
                if (!supported)
                    device.usbId = "1395:0000";
                result.devices = {device};
                return result;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController settings(discovery, transport());
        auto configuration = options(directory, 0);
        configuration.automatic = automatic;
        DeviceAccessSetup setup(discovery, settings, configuration);
        QTRY_VERIFY(!discovery.busy());
        QCoreApplication::processEvents();
        QVERIFY(!setup.busy());
        QVERIFY(!QFile::exists(directory.filePath("activated")));
        QVERIFY(!QFile::exists(directory.filePath("state.ini")));
    }

    void processFailure_data() {
        QTest::addColumn<int>("exitCode");
        QTest::newRow("authorization-failed") << 127;
        QTest::newRow("installer-failed") << 1;
        QTest::newRow("activation-incomplete") << 3;
        QTest::newRow("failed-to-start") << -1;
    }
    void processFailure() {
        QFETCH(int, exitCode);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult result;
                result.devices = {lockedDevice()};
                return result;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController settings(
            discovery,
            [](const DeviceSnapshot &,
               std::stop_token) -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                return std::unexpected(Settings::Error{Settings::ErrorCode::AccessDenied, "Denied"});
            });
        auto configuration = options(directory, exitCode);
        if (exitCode == -1)
            configuration.program = directory.filePath("missing-program");
        DeviceAccessSetup setup(discovery, settings, configuration);
        QSignalSpy installed(&setup, &DeviceAccessSetup::installed);
        QTRY_VERIFY(setup.canRetry());
        QVERIFY(!setup.message().isEmpty());
        QCOMPARE(installed.count(), exitCode == 3 ? 1 : 0);
        QTRY_VERIFY(!settings.busy());
        QVERIFY(!settings.ready());
        QVERIFY(settings.needsAccess());
        QSettings state(directory.filePath("state.ini"), QSettings::IniFormat);
        QVERIFY(state.value("automaticAttempted").toBool());
    }

    void atomicFixedRuleInstall() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const int fd = ::open(QFile::encodeName(directory.path()), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        QVERIFY(fd >= 0);
        const auto close = qScopeGuard([&] { ::close(fd); });
        QVERIFY(DeviceAccess::installRule(fd, DeviceAccess::Rule));
        QVERIFY(DeviceAccess::installRule(fd, DeviceAccess::Rule)); // Idempotent replacement.
        QFile rule(directory.filePath(DeviceAccess::RuleName));
        QVERIFY(rule.open(QIODevice::ReadOnly));
        QCOMPARE(rule.readAll(), QByteArray::fromStdString(std::string(DeviceAccess::Rule)));
        rule.close();
        struct stat info{};
        QCOMPARE(::fstatat(fd, DeviceAccess::RuleName, &info, AT_SYMLINK_NOFOLLOW), 0);
        QCOMPARE(info.st_mode & 0777, 0644u);
        QVERIFY(rule.remove());
        QFile victim(directory.filePath("other-file"));
        QVERIFY(victim.open(QIODevice::WriteOnly));
        victim.write("untouched");
        victim.close();
        QCOMPARE(::symlinkat("other-file", fd, DeviceAccess::RuleName), 0);
        QVERIFY(!DeviceAccess::installRule(fd, DeviceAccess::Rule));
        QVERIFY(victim.open(QIODevice::ReadOnly));
        QCOMPARE(victim.readAll(), QByteArray("untouched"));
        victim.close();
        QCOMPARE(::unlinkat(fd, DeviceAccess::RuleName, 0), 0);
        QCOMPARE(::mkdirat(fd, DeviceAccess::RuleName, 0700), 0);
        QVERIFY(!DeviceAccess::installRule(fd, DeviceAccess::Rule));
    }

    void helperReadOnlyModes() {
        QProcess child;
        const auto helper = qEnvironmentVariable("HEADSET_ACCESS_HELPER");
        QVERIFY(!helper.isEmpty());
        child.start(helper, {"--print-rule"});
        QVERIFY(child.waitForFinished());
        QCOMPARE(child.exitCode(), 0);
        QCOMPARE(child.readAllStandardOutput(), QByteArray::fromStdString(std::string(DeviceAccess::Rule)));
        child.start(helper, {"--install-to", "/not-allowed"});
        QVERIFY(child.waitForFinished());
        QCOMPARE(child.exitCode(), 1); // Arguments cannot redirect privileged writes.
    }
};

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    if (argc == 4 && QByteArray(argv[1]) == "--fake-setup") {
        const int code = QByteArray(argv[2]).toInt();
        if (code == 0) {
            QFile marker(QString::fromLocal8Bit(argv[3]));
            if (!marker.open(QIODevice::WriteOnly))
                return 1;
        }
        return code;
    }
    DeviceAccessTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "device_access_test.moc"
