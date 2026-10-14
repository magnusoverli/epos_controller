#include "FakeFirmwareNetwork.h"
#include "FakeSettingsTransport.h"
#include "FirmwareController.h"
#include "FirmwarePackageFixture.h"
#include "TestFixtures.h"
#include <QSignalSpy>
#include <QTest>
#include <atomic>
#include <thread>

class FirmwareTest : public QObject {
    Q_OBJECT
  private slots:
    void futureReleaseInstallation_data() {
        QTest::addColumn<QString>("target");
        QTest::newRow("next-minor") << QStringLiteral("1.6.0");
        QTest::newRow("next-major") << QStringLiteral("2.0.0");
    }
    void futureReleaseInstallation() {
        QFETCH(QString, target);
        const auto version = *Firmware::parseVersion(target.toStdString());
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice("1-1", "1-1#1")};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        hardware->firmwareVersion = Firmware::Version{1, 5, 619};
        SettingsController settings(discovery,
                                    [hardware](const DeviceSnapshot &, std::stop_token)
                                        -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                        return std::make_unique<FakeSettingsTransport>(hardware);
                                    });
        std::atomic<int> installs{0};
        FakeNetwork network;
        FirmwareController firmware(
            settings, true, &network, nullptr,
            [&](const DeviceSnapshot &, const QByteArray &data, const Firmware::Release &release,
                Firmware::Version installed, const Firmware::UpdateProgress &, std::stop_token) {
                ++installs;
                if (installed != Firmware::Version{1, 5, 619} || release.version != version ||
                    data != "future package")
                    return Firmware::UpdateOutcome{false, false,
                                                   QStringLiteral("Incorrect selected release")};
                std::lock_guard lock(hardware->mutex);
                hardware->firmwareVersion = version;
                return Firmware::UpdateOutcome{true, true, {}};
            });
        settings.setIdentityReadsEnabled(true);
        QTRY_COMPARE(network.requests, 1);
        network.catalog({FakeNetwork::release(target)});
        QTRY_VERIFY(firmware.canInstall());
        QCOMPARE(installs.load(), 0); // Finding a release does not install it.
        firmware.installUpdate(settings.connectionId(), target);
        QVERIFY(firmware.installing());
        QCOMPARE(network.reply->url(),
                 QUrl("https://update.eposstorage.com/deviceupdates/IMPACT_860T_ANC_" + target + ".zip"));
        network.reply->headers(200, 14);
        network.reply->publish("future package");
        network.reply->complete();
        QTRY_VERIFY(!firmware.installing());
        QCOMPARE(installs.load(), 1);
        QTRY_COMPARE(settings.firmwareVersion(), target);
        QVERIFY(firmware.installationMessage().contains(target + " installed and verified"));
        QTRY_COMPARE(network.requests, 3);
        network.catalog({FakeNetwork::release(target)});
        QCOMPARE(firmware.status(), QString("current"));
        QVERIFY(!firmware.canInstall());
    }
    void installationConfirmationInterlockAndFailure() {
        std::atomic<bool> finish{false};
        std::atomic<int> installs{0};
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice("1-1", "1-1#1")};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        SettingsController settings(discovery,
                                    [hardware](const DeviceSnapshot &, std::stop_token)
                                        -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                        return std::make_unique<FakeSettingsTransport>(hardware);
                                    });
        FakeNetwork network;
        FirmwareController firmware(
            settings, true, &network, nullptr,
            [&](const DeviceSnapshot &, const QByteArray &, const Firmware::Release &, Firmware::Version,
                const Firmware::UpdateProgress &progress, std::stop_token stop) {
                ++installs;
                (void)progress(Firmware::UpdateStage::Transferring, 10, 100);
                while (!finish && !stop.stop_requested())
                    std::this_thread::sleep_for(std::chrono::milliseconds{1});
                return Firmware::UpdateOutcome{false, true, QStringLiteral("Injected disconnect")};
            });
        settings.setIdentityReadsEnabled(true);
        QTRY_COMPARE(network.requests, 1);
        auto release = FakeNetwork::release();
        release["HashOfZipFile"] = "1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd";
        network.catalog({release});
        QTRY_VERIFY(firmware.canInstall());
        firmware.installUpdate("stale-connection", "1.5.619");
        firmware.installUpdate(settings.connectionId(), "1.5.618");
        QCOMPARE(network.requests, 1);
        QCOMPARE(installs.load(), 0);
        firmware.installUpdate(settings.connectionId(), "1.5.619");
        QVERIFY(firmware.installing());
        QVERIFY(settings.busy());
        QVERIFY(!firmware.canCheck());
        QCOMPARE(network.requests, 2);
        const auto sends = hardware->sends;
        settings.refresh();
        settings.apply("ancMode", 0);
        settings.factoryReset(settings.connectionId());
        QCOMPARE(hardware->sends, sends);
        firmware.installUpdate(settings.connectionId(), "1.5.619");
        QCOMPARE(network.requests, 2);
        network.reply->headers(200, 4);
        network.reply->publish("fake");
        network.reply->complete();
        QTRY_COMPARE(installs.load(), 1);
        QVERIFY(firmware.installing());
        finish = true;
        QTRY_VERIFY(!firmware.installing());
        QTRY_VERIFY(!settings.busy());
        QTRY_COMPARE(network.requests, 3);
        network.catalog({release});
        QVERIFY(!firmware.canInstall());
        QVERIFY(firmware.installationMessage().contains("Injected disconnect"));
        firmware.installUpdate(settings.connectionId(), "1.5.619");
        QCOMPARE(installs.load(), 1); // An uncertain result cannot be retried in this process.
        QCOMPARE(hardware->writes, 0);
    }
    void downloadDisconnectNeverStartsInstaller() {
        std::atomic<bool> present{true};
        int installs = 0;
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult scan;
                if (present)
                    scan.devices = {sampleSettingsDevice("1-1", "1-1#1")};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        SettingsController settings(discovery,
                                    [hardware](const DeviceSnapshot &, std::stop_token)
                                        -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                        return std::make_unique<FakeSettingsTransport>(hardware);
                                    });
        FakeNetwork network;
        FirmwareController firmware(settings, true, &network, nullptr,
                                    [&](const DeviceSnapshot &, const QByteArray &, const Firmware::Release &,
                                        Firmware::Version, const Firmware::UpdateProgress &,
                                        std::stop_token) {
                                        ++installs;
                                        return Firmware::UpdateOutcome{};
                                    });
        settings.setIdentityReadsEnabled(true);
        QTRY_COMPARE(network.requests, 1);
        auto release = FakeNetwork::release();
        release["HashOfZipFile"] = "1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd";
        network.catalog({release});
        firmware.installUpdate(settings.connectionId(), "1.5.619");
        QVERIFY(firmware.installing());
        present = false;
        discovery.refresh(true);
        QTRY_VERIFY(!firmware.installing());
        QVERIFY(!settings.busy());
        QCOMPARE(installs, 0);
        QVERIFY(!firmware.updateAvailable());
        QVERIFY(firmware.latestVersion().isEmpty());
        QVERIFY(firmware.installationMessage().contains("No firmware transfer"));
    }
    void automaticChecksAndConnectionChanges() {
        std::atomic<int> generation{1};
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult scan;
                if (generation != 0)
                    scan.devices = {sampleSettingsDevice("1-1", "1-1#" + QString::number(generation.load()))};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        SettingsController settings(discovery,
                                    [hardware](const DeviceSnapshot &, std::stop_token)
                                        -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                        return std::make_unique<FakeSettingsTransport>(hardware);
                                    });
        FakeNetwork network;
        FirmwareController firmware(settings, true, &network);
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(network.requests, 0);
        QVERIFY(!firmware.busy());
        QVERIFY(!firmware.canCheck());
        settings.setIdentityReadsEnabled(true);
        QTRY_COMPARE(network.requests, 1);
        QVERIFY(firmware.busy());
        QCOMPARE(network.reply->url(), Firmware::catalogUrl());
        QVERIFY(!network.reply->request().hasRawHeader("Token"));
        network.catalog({FakeNetwork::release("1.5.64"), FakeNetwork::release()});
        QVERIFY(firmware.updateAvailable());
        QCOMPARE(firmware.latestVersion(), QString("1.5.619"));
        QVERIFY(!firmware.checkedAt().isEmpty());
        firmware.checkForUpdates();
        firmware.checkForUpdates();
        QCOMPARE(network.requests, 2);
        QVERIFY(firmware.latestVersion().isEmpty());
        network.catalog({});
        QCOMPARE(firmware.status(), QString("error"));
        QVERIFY(!firmware.updateAvailable());
        firmware.checkForUpdates();
        QCOMPARE(network.requests, 3);
        generation = 0;
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(!firmware.busy());
        QVERIFY(firmware.latestVersion().isEmpty());
        QVERIFY(!firmware.updateAvailable());
        QVERIFY(!firmware.canCheck());
        generation = 2;
        discovery.refresh(true);
        QTRY_COMPARE(network.requests, 4);
        network.catalog({FakeNetwork::release("1.5.64")});
        QCOMPARE(firmware.status(), QString("current"));
        firmware.checkForUpdates();
        network.catalog({FakeNetwork::release("1.4.999")});
        QCOMPARE(firmware.status(), QString("ahead"));
        QVERIFY(!firmware.updateAvailable());
        firmware.checkForUpdates();
        network.reply->fail(QNetworkReply::SslHandshakeFailedError);
        QCOMPARE(firmware.status(), QString("error"));
        QVERIFY(firmware.latestVersion().isEmpty());
        QVERIFY(firmware.checkedAt().isEmpty());
        QCOMPARE(hardware->writes, 0);
        QCOMPARE(hardware->sends, 2); // Only the two explicitly enabled identity reads.
        const auto requests = network.requests;
        FirmwareController disabled(settings, false, &network);
        disabled.checkForUpdates();
        QVERIFY(!disabled.canCheck());
        QCOMPARE(network.requests, requests); // Automation previews cannot contact EPOS.
    }
    void catalogSelectionAndRejection() {
        auto parse = [](const QJsonArray &rows) {
            return Firmware::latestRelease(QJsonDocument(rows).toJson());
        };
        auto beta = FakeNetwork::release("99.0.0");
        beta["BuildType"] = "Beta";
        auto result =
            parse({FakeNetwork::release("1.5.64"), beta, FakeNetwork::release(), FakeNetwork::release()});
        QVERIFY(result);
        QCOMPARE(result->version.text(), std::string("1.5.619"));
        QVERIFY(!parse({}));
        QVERIFY(!parse({beta}));
        QVERIFY(!Firmware::latestRelease("<html>Not Found</html>"));
        QVERIFY(!parse({42}));
        for (const auto &key : {"Version", "FileUrl", "HashOfZipFile", "BuildType"}) {
            auto row = FakeNetwork::release();
            row.remove(key);
            QVERIFY(!parse({row}));
        }
        auto invalid = FakeNetwork::release("1.5.65536");
        QVERIFY(!parse({invalid}));
        invalid = FakeNetwork::release();
        invalid["FileUrl"] = "https://update.eposstorage.com/deviceupdates/OTHER_MODEL_1.5.619.zip";
        QVERIFY(!parse({invalid}));
        invalid = FakeNetwork::release();
        invalid["HashOfZipFile"] = QString(64, 'b');
        QVERIFY(!parse({FakeNetwork::release(), invalid}));
    }
    void catalogBodyLimit() {
        FakeNetwork network;
        Firmware::PackageDownload download(network);
        QSignalSpy finished(&download, &Firmware::PackageDownload::completed);
        download.startCatalog();
        QCOMPARE(network.reply->url(), Firmware::catalogUrl());
        network.reply->headers(200, 1024 * 1024 + 1);
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished.first()[1].toString().isEmpty());
    }
    void downloadSuccessAndPolicy() {
        FakeNetwork network;
        Firmware::PackageDownload download(network);
        QSignalSpy finished(&download, &Firmware::PackageDownload::completed);
        QSignalSpy progress(&download, &Firmware::PackageDownload::progress);
        const QUrl url("https://update.eposstorage.com/deviceupdates/synthetic-test.zip");
        download.start(url);
        QVERIFY(download.active());
        download.start(url);
        QCOMPARE(network.requests, 1);
        const auto request = network.reply->request();
        QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 int(QNetworkRequest::ManualRedirectPolicy));
        QCOMPARE(request.attribute(QNetworkRequest::CookieLoadControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(request.attribute(QNetworkRequest::CookieSaveControlAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QCOMPARE(request.attribute(QNetworkRequest::AuthenticationReuseAttribute).toInt(),
                 int(QNetworkRequest::Manual));
        QVERIFY(!request.hasRawHeader("Authorization"));
        QVERIFY(!request.hasRawHeader("Cookie"));
        QVERIFY(!request.hasRawHeader("Token"));
        QCOMPARE(request.rawHeader("Accept-Encoding"), QByteArray("identity"));
        const auto zip = SyntheticFirmware::zip();
        network.reply->headers(200, zip.size());
        network.reply->publish(zip.first(100));
        network.reply->publish(zip.sliced(100));
        network.reply->complete();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first()[0].toByteArray(), zip);
        QVERIFY(finished.first()[1].toString().isEmpty());
        QVERIFY(!download.active());
        QVERIFY(progress.count() >= 2);
        const auto offer = Firmware::parseOffer(QJsonDocument(SyntheticFirmware::offer()).toJson());
        QVERIFY(offer);
        QVERIFY(Firmware::inspectPackage(finished.first()[0].toByteArray(), *offer));
    }
    void rejectedDownload_data() {
        QTest::addColumn<QString>("scenario");
        for (const char *name : {"redirect", "http-error", "too-large-header", "too-large-stream", "short",
                                 "empty", "tls", "cancel", "timeout"})
            QTest::newRow(name) << QString::fromLatin1(name);
    }
    void rejectedDownload() {
        QFETCH(QString, scenario);
        FakeNetwork network;
        Firmware::PackageDownload download(network, nullptr, std::chrono::milliseconds{40});
        QSignalSpy finished(&download, &Firmware::PackageDownload::completed);
        download.start(QUrl("https://update.eposstorage.com/deviceupdates/synthetic-test.zip"));
        if (scenario == "redirect")
            network.reply->headers(302, -1, true);
        else if (scenario == "http-error")
            network.reply->headers(404);
        else if (scenario == "too-large-header")
            network.reply->headers(200, Firmware::PackageSizeLimit + 1);
        else if (scenario == "tls")
            network.reply->fail(QNetworkReply::SslHandshakeFailedError);
        else if (scenario == "cancel")
            download.cancel();
        else if (scenario == "too-large-stream") {
            network.reply->headers(200);
            for (int i = 0; i < 65 && download.active(); ++i)
                network.reply->publish(QByteArray(1024 * 1024, 'x'));
        } else if (scenario != "timeout") {
            network.reply->headers(200, scenario == "short" ? 100 : 0);
            if (scenario == "short")
                network.reply->publish("partial");
            network.reply->complete();
        }
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished.first()[0].toByteArray().isEmpty());
        QVERIFY(!finished.first()[1].toString().isEmpty());
        QVERIFY(!download.active());
        download.cancel();
        QCOMPARE(finished.count(), 1);
        QCOMPARE(network.requests, 1); // No redirect or automatic application retry.
    }
    void invalidUrlsNeverConnect() {
        FakeNetwork network;
        Firmware::PackageDownload download(network);
        QSignalSpy finished(&download, &Firmware::PackageDownload::completed);
        for (const char *url : {"http://update.eposstorage.com/deviceupdates/image.zip",
                                "file:///tmp/image.zip", "https://evil.invalid/deviceupdates/image.zip",
                                "https://user:secret@update.eposstorage.com/deviceupdates/image.zip",
                                "https://update.eposstorage.com/deviceupdates/image.zip?token=secret",
                                "https://update.eposstorage.com/other/image.zip"})
            download.start(QUrl(QString::fromLatin1(url)));
        QCOMPARE(network.requests, 0);
        QCOMPARE(finished.count(), 6);
    }
    void restartDuringCompletionProgress() {
        FakeNetwork network;
        Firmware::PackageDownload download(network);
        QSignalSpy finished(&download, &Firmware::PackageDownload::completed);
        const QUrl url("https://update.eposstorage.com/deviceupdates/synthetic-test.zip");
        download.start(url);
        network.reply->headers(200, 5);
        network.reply->publish("first");
        bool restarted = false;
        connect(&download, &Firmware::PackageDownload::progress, this, [&](qint64, qint64) {
            if (restarted)
                return;
            restarted = true;
            download.cancel();
            download.start(url);
        });
        network.reply->complete(); // Final drain emits progress; its observer replaces the request.
        QVERIFY(restarted);
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished.first()[1].toString().isEmpty());
        QVERIFY(download.active());
        network.reply->headers(200, 6);
        network.reply->publish("second");
        network.reply->complete();
        QCOMPARE(finished.count(), 2);
        QCOMPARE(finished.last()[0].toByteArray(), QByteArray("second"));
        QVERIFY(finished.last()[1].toString().isEmpty());
    }
};
QTEST_GUILESS_MAIN(FirmwareTest)
#include "firmware_test.moc"
