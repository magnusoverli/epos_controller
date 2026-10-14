#include "FakeSettingsTransport.h"
#include "SettingsController.h"
#include "TestFixtures.h"
#include <QMutex>
#include <QScopeGuard>
#include <QSemaphore>
#include <QTest>
#include <atomic>

class SettingsTest : public QObject {
    Q_OBJECT
  private slots:
    void editsStayResponsiveAndCoalesce() {
        auto hardware = std::make_shared<FakeSettingsDevice>();
        QSemaphore entered, resume;
        std::atomic<int> attempts{0};
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice()};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController controller(discovery,
                                      [&](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          if (++attempts == 2) {
                                              entered.release();
                                              resume.acquire();
                                          }
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        const auto releaseOnExit = qScopeGuard([&] { resume.release(); });
        controller.setActive(true);
        QTRY_VERIFY(controller.ready());
        controller.apply("sidetone", 84);
        QVERIFY(entered.tryAcquire(1, 2000));
        QVERIFY(controller.busy());
        QVERIFY(controller.ready()); // Last confirmed values stay available.
        auto control = [&](const char *key) { return controller.controls().value(key).toMap(); };
        QCOMPARE(control("sidetone")["display"].toString(), QString("33%"));
        QVERIFY(control("sidetone")["writable"].toBool());
        QVERIFY(control("ancMode")["writable"].toBool());
        QCOMPARE(control("audioLimiter")["display"].toString(), QString("Off"));
        controller.apply("ancMode", 0);
        controller.apply("sidetone", 86);
        controller.apply("sidetone", 128);
        controller.apply("ancMode", 1); // Coalesced back to confirmed state: no redundant output.
        controller.apply("audioLimiter", 1);
        controller.apply("audioLimiter", 2);
        QCOMPARE(control("sidetone")["display"].toString(), QString("50%"));
        QCOMPARE(control("audioLimiter")["display"].toString(), QString("AU limiter"));
        QVERIFY(!controller.reserveForUpdate(controller.connectionId()));
        controller.factoryReset(controller.connectionId());
        QCOMPARE(attempts.load(), 2); // One worker operation, not parallel USB writes.
        resume.release();
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(hardware->values.sidetone, 128);
        QCOMPARE(hardware->values.ancMode, 1);
        QCOMPARE(hardware->values.audioLimiter, 2);
        QCOMPARE(hardware->writes, 3); // First sidetone, latest sidetone, latest limiter.
        QCOMPARE(hardware->resets, 0);
        QCOMPARE(attempts.load(), 4);
    }
    void queuedDependenciesUseConfirmedPredecessor() {
        auto hardware = std::make_shared<FakeSettingsDevice>();
        hardware->values.ancMode = 0;
        QSemaphore entered, resume;
        std::atomic<int> attempts{0};
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice()};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController controller(discovery,
                                      [&](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          if (++attempts == 2) {
                                              entered.release();
                                              resume.acquire();
                                          }
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        const auto releaseOnExit = qScopeGuard([&] { resume.release(); });
        controller.setActive(true);
        QTRY_VERIFY(controller.ready());
        QVERIFY(!controller.controls()["adaptiveAnc"].toMap()["writable"].toBool());
        controller.apply("ancMode", 1);
        QVERIFY(entered.tryAcquire(1, 2000));
        QVERIFY(controller.controls()["adaptiveAnc"].toMap()["writable"].toBool());
        controller.apply("adaptiveAnc", 1);
        QVERIFY(!controller.controls()["ancLevel"].toMap()["writable"].toBool());
        QVERIFY(!controller.controls()["ancLevel"].toMap()["reason"].toString().isEmpty());
        controller.apply("adaptiveAnc", 0);
        controller.apply("ancLevel", 1);
        resume.release();
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(hardware->values.ancMode, 1);
        QCOMPARE(hardware->values.adaptiveAnc, 0);
        QCOMPARE(hardware->values.ancLevel, 1);
        QCOMPARE(hardware->writes, 2);
    }
    void queuedEditsDoNotSurviveFailuresOrReconnection_data() {
        QTest::addColumn<bool>("disconnect");
        QTest::newRow("failed-output") << false;
        QTest::newRow("new-connection") << true;
    }
    void queuedEditsDoNotSurviveFailuresOrReconnection() {
        QFETCH(bool, disconnect);
        auto hardware = std::make_shared<FakeSettingsDevice>();
        auto replacement = std::make_shared<FakeSettingsDevice>();
        hardware->failWrite = true;
        QSemaphore entered, resume;
        std::atomic<int> attempts{0}, generation{1};
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice("1-1", "1-1#" + QString::number(generation.load()))};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController controller(
            discovery,
            [&](const DeviceSnapshot &device,
                std::stop_token) -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                if (++attempts == 2) {
                    entered.release();
                    resume.acquire();
                }
                return std::make_unique<FakeSettingsTransport>(device.connectionId == "1-1#1" ? hardware
                                                                                              : replacement);
            });
        const auto releaseOnExit = qScopeGuard([&] { resume.release(); });
        controller.setActive(true);
        QTRY_VERIFY(controller.ready());
        controller.apply("sidetone", 84);
        QVERIFY(entered.tryAcquire(1, 2000));
        controller.apply("audioLimiter", 2);
        if (disconnect) {
            generation = 2;
            discovery.refresh(true);
            QTRY_COMPARE(controller.connectionId(), QString("1-1#2"));
        }
        resume.release();
        QTRY_VERIFY(!controller.busy());
        if (disconnect) {
            QVERIFY(controller.ready());
            QCOMPARE(hardware->writes, 0);
            QCOMPARE(replacement->writes, 0);
        } else {
            QVERIFY(!controller.ready());
            QCOMPARE(hardware->writes, 1);
            QVERIFY(!controller.controls()["sidetone"].toMap()["error"].toString().isEmpty());
            QVERIFY(!controller.controls()["audioLimiter"].toMap()["error"].toString().isEmpty());
            QCOMPARE(controller.controls()["audioLimiter"].toMap()["display"].toString(), QString("Off"));
            controller.refresh();
            QTRY_VERIFY(controller.ready());
            QCOMPARE(hardware->values.audioLimiter, 0);
            QCOMPARE(hardware->writes, 1); // Recovery reads never retry queued writes.
        }
    }
    void firmwareIdentityAndReconnection() {
        std::atomic<int> generation{1};
        auto hardware = std::make_shared<FakeSettingsDevice>();
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult scan;
                if (generation != 0)
                    scan.devices = {sampleSettingsDevice("1-1", "1-1#" + QString::number(generation.load()))};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        SettingsController controller(discovery,
                                      [hardware](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(hardware->sends, 0);
        controller.setIdentityReadsEnabled(true);
        QTRY_COMPARE(controller.firmwareVersion(), QString("1.5.64"));
        QVERIFY(!controller.ready()); // Metadata does not invent a settings snapshot.
        controller.setActive(true);
        QTRY_VERIFY(controller.ready());
        controller.factoryReset("1-1#1");
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(hardware->resets, 1);
        generation = 0;
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(controller.firmwareVersion().isEmpty());
        QVERIFY(!controller.ready());
        generation = 2;
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QTRY_VERIFY(controller.ready());
        QTRY_COMPARE(controller.firmwareVersion(), QString("1.5.64"));
        QCOMPARE(discovery.selectedDevice()->connectionId(), QString("1-1#2"));
        controller.factoryReset("1-1#1");
        QCOMPARE(hardware->resets, 1); // Neither reconnection nor old confirmation repeats reset.
    }
    void factoryResetRequiresCurrentConnection() {
        std::atomic<int> generation{1};
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice("1-1", "1-1#" + QString::number(generation.load()))};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        SettingsController controller(discovery,
                                      [hardware](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        QTRY_VERIFY(!discovery.busy());
        controller.factoryReset("1-1#1");
        QCOMPARE(hardware->resets, 0); // No snapshot yet.
        controller.refresh();
        QTRY_VERIFY(controller.ready());
        controller.factoryReset("");
        controller.factoryReset("1-1#old");
        QCOMPARE(hardware->resets, 0);
        controller.factoryReset("1-1#1");
        controller.factoryReset("1-1#1"); // Busy: never send twice.
        QTRY_VERIFY(controller.ready());
        QCOMPARE(hardware->resets, 1);
        QCOMPARE(hardware->values, hardware->valuesAfterReset);
        QVERIFY(controller.message().contains("Reset acknowledged"));
        generation = 2;
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        discovery.selectDevice("1-1#2");
        controller.refresh();
        QTRY_VERIFY(controller.ready());
        controller.factoryReset("1-1#1"); // Confirmation cannot follow a replug/replacement.
        QCOMPARE(hardware->resets, 1);
        hardware->failReset = true;
        controller.factoryReset("1-1#2");
        QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.message().contains("Reset may have applied"));
        QCOMPARE(hardware->resets, 2);
        controller.factoryReset("1-1#2"); // Must read current state first.
        QCOMPARE(hardware->resets, 2);
        controller.refresh();
        QTRY_VERIFY(controller.ready());
        QCOMPARE(hardware->resets, 2); // Refresh reads; it never retries reset.
    }

    void readApplyAndDependencies() {
        const auto device = sampleSettingsDevice();
        DeviceDiscovery discovery(
            [&](bool) {
                ScanResult scan;
                scan.devices = {device};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        SettingsController controller(discovery,
                                      [hardware](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(controller.available());
        QVERIFY(!controller.ready());
        QCOMPARE(hardware->sends, 0); // Loading starts when the device-settings view becomes active.
        controller.setActive(true);
        QVERIFY(controller.busy());
        controller.refresh(); // At most one operation; refresh does not queue another job.
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(hardware->sends, int(Settings::Features.size()));
        controller.setActive(true); // Repeated activation and unchanged discovery do not queue reads.
        discovery.refresh();
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(hardware->sends, int(Settings::Features.size()));
        auto sidetone = controller.controls().value("sidetone").toMap();
        QCOMPARE(sidetone.value("display").toString(), QString("32%"));
        controller.applySidetonePercent(33);
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(hardware->writes, 1);
        QCOMPARE(hardware->values.sidetone, 84);
        controller.apply("adaptiveAnc", 1);
        QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.controls().value("ancLevel").toMap().value("writable").toBool());
        controller.apply("ancLevel", 1);
        QVERIFY(!controller.busy());
        QCOMPARE(hardware->writes, 2);
        controller.setActive(false);
        {
            std::lock_guard lock(hardware->mutex);
            hardware->values.sidetone = 82;
        }
        controller.setActive(true);
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.controls().value("sidetone").toMap().value("display").toString(), QString("32%"));
        QCOMPARE(hardware->writes, 2); // Reopening reads external changes without writing.
        controller.apply("factoryReset", 1);
        QVERIFY(!controller.busy());
        QCOMPARE(hardware->writes, 2);
    }

    void percentageReadbackIsLossless() {
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice()};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        hardware->values = {81, 2, 0, 0};
        SettingsController controller(discovery,
                                      [hardware](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        QTRY_VERIFY(!discovery.busy());
        controller.refresh();
        QTRY_VERIFY(controller.ready());
        QCOMPARE(controller.controls().value("sidetone").toMap().value("display").toString(),
                 QString("32% (raw 81)"));
        QCOMPARE(controller.controls().value("sidetone").toMap().value("percent").toInt(), 32);
        QCOMPARE(controller.controls().value("ancMode").toMap().value("display").toString(),
                 QString("TalkThrough"));
        QCOMPARE(controller.controls().value("ancLevel").toMap().value("display").toString(), QString("Low"));
        QVERIFY(!controller.controls().value("ancLevel").toMap().value("writable").toBool());
        QVERIFY(!controller.controls().value("adaptiveAnc").toMap().value("writable").toBool());
        for (const int percent : {-1, 32, 101})
            controller.applySidetonePercent(percent);
        QCOMPARE(hardware->sends, int(Settings::Features.size()));
        QCOMPARE(hardware->values.sidetone, 81);
        controller.applySidetonePercent(50);
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(hardware->values.sidetone, 128);
        QCOMPARE(hardware->writes, 1);
        {
            std::lock_guard lock(hardware->mutex);
            hardware->values.sidetone = 256; // Unsupported future firmware value.
        }
        controller.refresh();
        QTRY_VERIFY(!controller.busy());
        const auto control = controller.controls().value("sidetone").toMap();
        QCOMPARE(control.value("display").toString(), QString("Unknown (raw 256)"));
        QCOMPARE(control.value("percent").toInt(), -1);
        QVERIFY(!control.value("writable").toBool());
        controller.applySidetonePercent(50);
        QCOMPARE(hardware->writes, 1);
    }

    void permissionErrorAndRetry() {
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice()};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        int attempts = 0;
        SettingsController controller(
            discovery,
            [&attempts](const DeviceSnapshot &,
                        std::stop_token) -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                ++attempts;
                return std::unexpected(
                    Settings::Error{Settings::ErrorCode::AccessDenied, "Permission denied"});
            });
        controller.setActive(true); // Active before asynchronous discovery completes.
        QTRY_VERIFY(!discovery.busy());
        QTRY_COMPARE(controller.status(), QString("error"));
        QCOMPARE(attempts, 1);
        controller.setActive(true);
        discovery.refresh();
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(attempts, 1); // Permission failures do not trigger an automatic retry loop.
        for (int i = 0; i < 2; ++i) {
            controller.refresh();
            QTRY_VERIFY(!controller.busy());
            QVERIFY(!controller.ready());
            QCOMPARE(controller.status(), QString("error"));
            QVERIFY(controller.needsAccess());
            QVERIFY(controller.message().contains("access"));
        }
    }

    void staleConnectionCannotPublish_data() {
        QTest::addColumn<bool>("leavePage");
        QTest::newRow("read-replacement") << false;
        QTest::newRow("defer-until-reopened") << true;
    }

    void staleConnectionCannotPublish() {
        QFETCH(bool, leavePage);
        QMutex mutex;
        auto device = sampleSettingsDevice();
        DeviceDiscovery discovery(
            [&](bool) {
                QMutexLocker lock(&mutex);
                ScanResult scan;
                scan.devices = {device};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        QSemaphore entered, resume;
        int attempts = 0;
        SettingsController controller(discovery,
                                      [&](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          if (++attempts == 1) {
                                              entered.release();
                                              resume.acquire();
                                          }
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        const auto releaseOnExit = qScopeGuard([&] { resume.release(); });
        QTRY_VERIFY(!discovery.busy());
        controller.setActive(true);
        check(entered.tryAcquire(1, 2000), "worker entered transport factory");
        {
            QMutexLocker lock(&mutex);
            device.connectionId = "1-1#2";
        }
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(controller.available());
        QVERIFY(controller.busy());
        discovery.selectDevice("1-1#2");
        QVERIFY(controller.available());
        QVERIFY(!controller.ready());
        QVERIFY(controller.busy()); // The replacement read waits for the cancelled generation.
        if (leavePage)
            controller.setActive(false);
        resume.release();
        QTRY_VERIFY(!controller.busy());
        if (leavePage) {
            QCOMPARE(attempts, 1);
            QCOMPARE(hardware->sends, 0);
            QVERIFY(!controller.ready());
            controller.setActive(true);
            QTRY_VERIFY(!controller.busy());
        }
        QCOMPARE(attempts, 2);
        QCOMPARE(hardware->sends, int(Settings::Features.size())); // Only the new selection reaches the wire.
        QVERIFY(controller.ready());
    }

    void uncertainWriteClearsReadback() {
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult scan;
                scan.devices = {sampleSettingsDevice()};
                return scan;
            },
            nullptr, std::chrono::milliseconds::zero());
        auto hardware = std::make_shared<FakeSettingsDevice>();
        hardware->failWrite = true;
        SettingsController controller(discovery,
                                      [hardware](const DeviceSnapshot &, std::stop_token)
                                          -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                                          return std::make_unique<FakeSettingsTransport>(hardware);
                                      });
        QTRY_VERIFY(!discovery.busy());
        controller.setActive(true);
        QTRY_VERIFY(controller.ready());
        controller.apply("sidetone", 84);
        QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.message().contains("may have changed"));
        controller.apply("sidetone", 82);
        QCOMPARE(hardware->writes, 1); // No writes until a fresh successful read.
        controller.refresh();
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(controller.controls().value("sidetone").toMap().value("display").toString(), QString("33%"));
    }

    void interfaceSelection() {
        auto device = sampleSettingsDevice();
        QVERIFY(Settings::supportsSettings(device));
        device.hidInterfaces[0].interfaceNumber = "04";
        QVERIFY(!Settings::supportsSettings(device));
        device.hidInterfaces[0].interfaceNumber = "03";
        device.hidInterfaces.append(device.hidInterfaces.first());
        QVERIFY(!Settings::supportsSettings(device));
        device.hidInterfaces.removeLast();
        device.usbId = "1395:ffff";
        QVERIFY(!Settings::supportsSettings(device));
        device.usbId = "1395:018e";
        auto result = Settings::openHidTransport(device, {});
        QVERIFY(!result); // No sysfs connection identity, never opens an arbitrary node.
        QCOMPARE(result.error().code, Settings::ErrorCode::Disconnected);
    }
};
QTEST_GUILESS_MAIN(SettingsTest)
#include "settings_test.moc"
