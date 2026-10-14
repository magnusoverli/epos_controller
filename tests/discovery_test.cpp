#include "DeviceDiscovery.h"
#include "Diagnostics.h"
#include "SysfsReader.h"
#include "TestFixtures.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTest>
#include <atomic>
#include <sys/stat.h>
#include <unistd.h>

class DiscoveryTest : public QObject {
    Q_OBJECT
  private slots:
    void filteringAndGeneration() {
        Fixture fixture;
        const auto usb = fixture.addDevice();
        fixture.addHid(usb);
        // FIFOs would block if the scanner read attributes it should skip.
        const auto ignored = fixture.paths.usb + "/1-1:1.4";
        QVERIFY(QDir().mkpath(ignored));
        QCOMPARE(::mkfifo(QFile::encodeName(ignored + "/idVendor").constData(), 0600), 0);
        writeFixture(fixture.paths.usb + "/2-1/idVendor", "1234");
        QCOMPARE(::mkfifo(QFile::encodeName(fixture.paths.usb + "/2-1/idProduct").constData(), 0600), 0);
        SysfsDeviceScanner scanner(fixture.paths);
        const auto first = scanner();
        QCOMPARE(first.status, ScanStatus::Ready);
        QCOMPARE(first.devices.size(), 1);
        QCOMPARE(first.devices.first().hidInterfaces.size(), 1);
        QCOMPARE(scanner().devices, first.devices);
        QCOMPARE(scanner(true).devices, first.devices);
        writeFixture(usb + "/devnum", "2");
        const auto replaced = scanner();
        QVERIFY(replaced.devices.first().connectionId != first.devices.first().connectionId);
        QVERIFY(QDir(usb).removeRecursively());
        QVERIFY(scanner().devices.isEmpty());
        fixture.addDevice();
        const auto reconnected = scanner();
        QVERIFY(reconnected.devices.first().connectionId != first.devices.first().connectionId);
    }

    void structuredFailures() {
        Fixture fixture;
        SysfsDeviceScanner scanner(fixture.paths);
        QCOMPARE(scanner().status, ScanStatus::Ready);
        QVERIFY(QDir(fixture.paths.usb).removeRecursively());
        const auto unavailable = scanner();
        QCOMPARE(unavailable.status, ScanStatus::Unavailable);
        QCOMPARE(unavailable.issues.first().code, ReadError::Missing);
        const auto usb = fixture.addDevice();
        writeFixture(usb + "/product", QByteArray(4097, 'x'));
        const auto partial = scanner();
        QCOMPARE(partial.status, ScanStatus::Partial);
        QCOMPARE(partial.devices.first().issues.first().code, ReadError::TooLarge);
        QCOMPARE(partial.devices.first().name, QString("EPOS IMPACT 860T ANC"));
        const auto json = QJsonDocument::fromJson(Diagnostics::toJson(unavailable)).object();
        QCOMPARE(json["scanStatus"].toString(), QString("unavailable"));
        QCOMPARE(json["issues"].toArray().first().toObject()["code"].toString(), QString("missing"));
        QVERIFY(!json["capturedAt"].toString().isEmpty());
    }

    void readLimitsAndPermissions() {
        Fixture fixture;
        const auto path = fixture.root.path() + "/attribute";
        writeFixture(path, QByteArray(16, 'a'));
        QVERIFY(SysfsReader::read(path, 16));
        writeFixture(path, QByteArray(17, 'a'));
        QCOMPARE(SysfsReader::read(path, 16).error().code, ReadError::TooLarge);
        writeFixture(path, {});
        QCOMPARE(SysfsReader::read(path, 16).error().code, ReadError::Empty);
        writeFixture(path, "  \n");
        QCOMPARE(SysfsReader::text(path).error().code, ReadError::Empty);
        if (::geteuid() == 0)
            QSKIP("Permission denial requires an unprivileged test process");
        QVERIFY(QFile::setPermissions(path, {}));
        QCOMPARE(SysfsReader::read(path, 16).error().code, ReadError::PermissionDenied);
        QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    }

    void notificationsSelectionAndStaleState() {
        QMutex mutex;
        ScanResult next;
        next.devices = {sampleDevice()};
        DeviceDiscovery discovery(
            [&](bool) {
                QMutexLocker lock(&mutex);
                return next;
            },
            nullptr, std::chrono::milliseconds::zero());
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(discovery.selectedDevice()->connectionId(), QString("1-1#1"));
        QSignalSpy changed(&discovery, &DeviceDiscovery::devicesChanged);
        auto refresh = [&] { discovery.refresh(); };
        refresh();
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(changed.count(), 0);
        {
            QMutexLocker lock(&mutex);
            next.devices.prepend(sampleDevice("0-1", "0-1#2"));
        }
        refresh();
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(discovery.selectedDevice()->connectionId(), QString("1-1#1"));
        {
            QMutexLocker lock(&mutex);
            next.status = ScanStatus::Unavailable;
            next.devices.clear();
        }
        refresh();
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(discovery.stale());
        QVERIFY(!discovery.selectedDevice()->connected());
        QCOMPARE(discovery.snapshot().devices.size(), 2);
        {
            QMutexLocker lock(&mutex);
            next.status = ScanStatus::Ready;
            next.devices = {sampleDevice("1-1", "1-1#3")};
        }
        refresh();
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(discovery.selectedDevice()->connected()); // Selection follows port, operations use new ID.
        QCOMPARE(discovery.selectedDevice()->connectionId(), QString("1-1#3"));
        discovery.selectDevice("1-1#3");
        QVERIFY(discovery.selectedDevice()->connected());
        discovery.selectDevice("missing");
        QCOMPARE(discovery.selectedDevice()->connectionId(), QString("1-1#3"));
    }

    void timerUsesWorkerAndCoalesces() {
        std::atomic<int> calls = 0;
        std::atomic<bool> offMain = false;
        QSemaphore entered, release;
        DeviceDiscovery discovery(
            [&](bool) {
                offMain = QThread::currentThread() != thread();
                const auto call = ++calls;
                if (call == 1) {
                    entered.release();
                    release.acquire();
                }
                ScanResult result;
                result.devices = {sampleDevice("1-1", QString::number(call))};
                return result;
            },
            nullptr, std::chrono::milliseconds::zero());
        QVERIFY(entered.tryAcquire(1, 3000));
        QSignalSpy changed(&discovery, &DeviceDiscovery::devicesChanged);
        for (int i = 0; i < 20; ++i)
            discovery.refresh(true);
        release.release();
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(offMain);
        QCOMPARE(calls.load(), 2);
        QCOMPARE(changed.count(), 1); // Invalidated result never published.
        QCOMPARE(discovery.selectedDevice()->connectionId(), QString("2"));

        DeviceDiscovery polling([](bool) { return ScanResult{}; }, nullptr, std::chrono::milliseconds{10});
        QSignalSpy finished(&polling, &DeviceDiscovery::scanFinished);
        QTRY_VERIFY(finished.count() >= 2);
    }

    void joinsOwnedScanner() {
        auto value = std::make_unique<int>(42);
        DeviceDiscovery discovery(
            [owned = std::move(value)](bool) {
                check(*owned == 42, "move-only scanner ownership");
                return ScanResult{};
            },
            nullptr, std::chrono::milliseconds::zero());
        QTRY_VERIFY(!discovery.busy());
    }

    void pollsDoNotQueueBehindSlowScan() {
        std::atomic<int> calls = 0;
        QSemaphore entered, release;
        DeviceDiscovery discovery(
            [&](bool) {
                if (++calls == 1) {
                    entered.release();
                    release.acquire();
                }
                return ScanResult{};
            },
            nullptr, std::chrono::milliseconds{1});
        const bool started = entered.tryAcquire(1, 3000);
        if (!started)
            release.release();
        QVERIFY(started);
        int ticks = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, this, [&] { ++ticks; });
        timer.start(1);
        QTRY_VERIFY(ticks >= 3);
        int callsAtFirstCompletion = 0;
        connect(&discovery, &DeviceDiscovery::scanFinished, this, [&] {
            if (callsAtFirstCompletion == 0)
                callsAtFirstCompletion = calls.load();
        });
        release.release();
        QTRY_VERIFY(callsAtFirstCompletion > 0);
        QCOMPARE(callsAtFirstCompletion, 1);
    }
};

QTEST_GUILESS_MAIN(DiscoveryTest)
#include "discovery_test.moc"
