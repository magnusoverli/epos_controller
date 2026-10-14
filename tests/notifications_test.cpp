#include "MuteEventFixtures.h"
#include "NotificationController.h"
#include "TestFixtures.h"
#include <QDBusConnection>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <sys/socket.h>
#include <unistd.h>

namespace {
struct Input {
    int peer = -1;
    int opens = 0;
    ~Input() { closePeer(); }
    void closePeer() {
        if (peer >= 0)
            ::close(peer);
        peer = -1;
    }
    Settings::Result<int> open(const DeviceSnapshot &) {
        closePeer();
        int fds[2];
        if (::socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) != 0)
            return std::unexpected(Settings::Error{Settings::ErrorCode::Io, "socketpair failed"});
        ++opens;
        peer = fds[0];
        return fds[1];
    }
    void report(int index) {
        const auto bytes = CapturedSettings::report(CapturedMuteEvents::Reports[index].hex);
        QCOMPARE(::send(peer, bytes.data(), bytes.size(), MSG_NOSIGNAL), ssize_t(bytes.size()));
    }
};
int selected(const NotificationController &controller, const char *key) {
    return controller.controls()[key].toMap()["currentIndex"].toInt();
}
} // namespace

class DesktopEndpoint : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
  public:
    int calls = 0;
    QVariantMap lastHints;
  public slots:
    uint Notify(const QString &app, uint replacement, const QString &icon, const QString &title,
                const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout) {
        Q_UNUSED(app)
        Q_UNUSED(replacement)
        Q_UNUSED(icon)
        Q_UNUSED(title)
        Q_UNUSED(body)
        Q_UNUSED(actions)
        Q_UNUSED(timeout)
        ++calls;
        lastHints = hints;
        return uint(calls);
    }
};

class NotificationsTest : public QObject {
    Q_OBJECT
  private slots:
    void soundRoutingAndCue() {
        using Notifications::AlertSound;
        const QByteArray sink =
            R"({"name":"headset-output","properties":{"device.vendor.id":"0x1395","device.product.id":"018e","sysfs.path":"/devices/usb3/3-1/3-1.1/3-1.1:1.0/sound/card4"}})";
        QCOMPARE(AlertSound::headsetSink('[' + sink + ']', "3-1.1"), QString("headset-output"));
        QVERIFY(AlertSound::headsetSink('[' + sink + ']', "3-1.10").isEmpty());
        QVERIFY(AlertSound::headsetSink('[' + sink + ',' + sink + ']', "3-1.1").isEmpty());
        QVERIFY(AlertSound::headsetSink("not json", "3-1.1").isEmpty());
        const auto cue = AlertSound::tone();
        QCOMPARE(cue.size(), 38400);
        QVERIFY(cue != QByteArray(cue.size(), '\0'));
        QCOMPARE(cue.first(2), QByteArray(2, '\0'));
        QCOMPARE(cue.last(4800), QByteArray(4800, '\0'));
    }
    void capturedEvents() {
        for (const auto &fixture : CapturedMuteEvents::Reports) {
            const auto report = CapturedSettings::report(fixture.hex);
            QVERIFY(Notifications::decode(report) == fixture.event);
            QVERIFY(!EposProtocol::decode(report)); // Events cannot satisfy a settings transaction.
            for (std::size_t n = 0; n < report.size(); ++n)
                QVERIFY(!Notifications::decode(std::span(report).first(n)));
            for (std::size_t offset = 0; offset < report.size(); ++offset) {
                auto bad = report;
                bad[offset] ^= 0x80;
                QVERIFY(!Notifications::decode(bad));
            }
            for (const auto offset : {13, 15, 17, 18, 19, 20}) {
                auto bad = report;
                bad[static_cast<std::size_t>(offset)] = 3;
                bad[11] = EposProtocol::checksum(std::span(bad).subspan(13, 8));
                QVERIFY(!Notifications::decode(bad));
            }
        }
        QVERIFY(!Notifications::decode(CapturedSettings::report(CapturedSettings::Pairs[0].response)));
    }
    void speechRateLimit() {
        using namespace Notifications;
        const auto start = Policy::Clock::time_point{};
        for (const int minutes : {0, 1, 5, 10}) {
            Policy policy({Mode::Visual, minutes});
            QVERIFY(!policy.observe({EventKind::MicrophoneMuted, true}, start));
            QVERIFY(policy.observe({EventKind::SpeakingWhileMuted, true}, start));
            QVERIFY(!policy.observe({EventKind::SpeakingWhileMuted, true}, start));
            QVERIFY(!policy.observe({EventKind::SpeakingWhileMuted, false}, start));
            QCOMPARE(policy.observe({EventKind::SpeakingWhileMuted, true}, start + std::chrono::seconds(1)),
                     minutes == 0);
            QVERIFY(!policy.observe({EventKind::SpeakingWhileMuted, false}, start + std::chrono::seconds(2)));
            // A mute/unmute cycle does not bypass the per-device cooldown.
            QVERIFY(!policy.observe({EventKind::MicrophoneMuted, false}, start + std::chrono::seconds(2)));
            if (minutes > 0) {
                QVERIFY(
                    !policy.observe({EventKind::SpeakingWhileMuted, true},
                                    start + std::chrono::minutes(minutes) - std::chrono::milliseconds(1)));
                QVERIFY(!policy.observe({EventKind::SpeakingWhileMuted, false},
                                        start + std::chrono::minutes(minutes)));
                QVERIFY(policy.observe({EventKind::SpeakingWhileMuted, true},
                                       start + std::chrono::minutes(minutes)));
            }
        }
        for (const auto prefs :
             {Preferences{}, Preferences{Mode::Visual, 2}, Preferences{static_cast<Mode>(9), 0}}) {
            Policy policy(prefs);
            QVERIFY(!policy.observe({EventKind::SpeakingWhileMuted, true}, start));
        }
    }
    void preferencesAndListenerLifecycle() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("notifications.ini");
        ScanResult snapshot;
        snapshot.devices = {sampleSettingsDevice()};
        DeviceDiscovery discovery([&](bool) { return snapshot; }, nullptr, std::chrono::milliseconds::zero());
        Input input;
        QList<bool> sent;
        NotificationController controller(
            discovery, true, path, [&](const DeviceSnapshot &device) { return input.open(device); },
            [&](bool audio, auto done) {
                sent.append(audio);
                done({});
            });
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(input.opens, 0); // Off is the local default; no HID access.
        controller.apply("muteInterval", 5);
        QCOMPARE(selected(controller, "muteInterval"), 0);
        controller.apply("muteNotification", 99);
        QCOMPARE(input.opens, 0);
        controller.apply("muteNotification", 1);
        QCOMPARE(input.opens, 1);
        input.report(2); // Muting alone does not send a notification.
        input.report(0);
        QTRY_COMPARE(sent.size(), 1);
        QCOMPARE(sent[0], false);
        input.report(0); // Duplicate true must not create a second toast.
        input.report(1);
        input.report(0);
        QTRY_COMPARE(sent.size(), 2);
        controller.apply("muteInterval", 5);
        QCOMPARE(selected(controller, "muteInterval"), 2);
        controller.apply("muteNotification", 2);
        input.report(0);
        QTRY_COMPARE(sent.size(), 3);
        QCOMPARE(sent.back(), true);
        input.closePeer();
        QTRY_VERIFY(controller.canRetry());
        const auto opens = input.opens;
        controller.retry();
        QCOMPARE(input.opens, opens + 1);
        QVERIFY(!controller.canRetry());
        controller.apply("muteNotification", 0);
        const auto stopped = input.opens;
        controller.retry();
        QCOMPARE(input.opens, stopped);
        controller.apply("muteNotification", 2);
        {
            Input fresh;
            NotificationController reload(
                discovery, true, path, [&](const DeviceSnapshot &device) { return fresh.open(device); },
                [](bool, auto done) { done({}); });
            QCOMPARE(fresh.opens, 1);
            QCOMPARE(selected(reload, "muteNotification"), 2);
            QCOMPARE(selected(reload, "muteInterval"), 2);
        }
        NotificationController automation(discovery, false, path,
                                          [&](const DeviceSnapshot &) -> Settings::Result<int> {
                                              qFatal("Automation must never open event HID");
                                          });
        automation.apply("muteNotification", 0);
        QCOMPARE(selected(automation, "muteNotification"), 2);
        QVERIFY(!automation.controls()["muteNotification"].toMap()["writable"].toBool());
        snapshot.devices.clear();
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(!controller.controls()["muteNotification"].toMap()["writable"].toBool());
        QCOMPARE(selected(controller, "muteNotification"), 0);
        const auto beforeReplug = input.opens;
        snapshot.devices = {sampleSettingsDevice("2-2", "2-2#1")};
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        discovery.selectDevice("2-2#1");
        QCOMPARE(selected(controller, "muteNotification"), 0); // Another USB port has separate preferences.
        QCOMPARE(input.opens, beforeReplug);
        snapshot.devices = {sampleSettingsDevice("1-1", "1-1#2")};
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        discovery.selectDevice("1-1#2");
        QCOMPARE(selected(controller, "muteNotification"), 2);
        QCOMPARE(input.opens, beforeReplug + 1);
    }
    void invalidPreferencesAndAccess() {
        QTemporaryDir directory;
        const auto path = directory.filePath("notifications.ini");
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.beginGroup(QString::fromLatin1(QByteArray("1395:018e/1-1").toHex()));
            settings.setValue("mode", 2);
            settings.setValue("intervalMinutes", "garbled");
            settings.sync();
        }
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult s;
                s.devices = {sampleSettingsDevice()};
                return s;
            },
            nullptr, std::chrono::milliseconds::zero());
        int opens = 0;
        NotificationController controller(
            discovery, true, path, [&](const DeviceSnapshot &) -> Settings::Result<int> {
                ++opens;
                return std::unexpected(Settings::Error{Settings::ErrorCode::AccessDenied, "access denied"});
            });
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(selected(controller, "muteNotification"), 0);
        QCOMPARE(opens, 0);
        controller.apply("muteNotification", 1);
        QCOMPARE(opens, 1);
        QVERIFY(controller.canRetry());
        QVERIFY(controller.message().contains("access denied"));
        QVERIFY(!Settings::openHidEventInput(sampleSettingsDevice())); // Missing connection identity.
        QVERIFY(!Settings::openHidEventInput(sampleDevice()));         // Unsupported descriptor.
        QFile blocker(directory.filePath("blocked"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        NotificationController unwritable(discovery, true, directory.filePath("blocked/preferences.ini"));
        unwritable.apply("muteNotification", 1);
        QCOMPARE(selected(unwritable, "muteNotification"), 0);
        QVERIFY(unwritable.message().contains("Could not save"));
    }
    void deliveryFailureAndStaleCompletion() {
        QTemporaryDir directory;
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult s;
                s.devices = {sampleSettingsDevice()};
                return s;
            },
            nullptr, std::chrono::milliseconds::zero());
        Input input;
        NotificationController::Completion completion;
        NotificationController controller(
            discovery, true, directory.filePath("notifications.ini"),
            [&](const DeviceSnapshot &device) { return input.open(device); },
            [&](bool, auto done) { completion = std::move(done); });
        QTRY_VERIFY(!discovery.busy());
        controller.apply("muteNotification", 1);
        input.report(0);
        QTRY_VERIFY(bool(completion));
        completion("no desktop service");
        QVERIFY(controller.canRetry());
        QVERIFY(controller.message().contains("no desktop service"));
        controller.retry();
        completion("late failure"); // Completion from the retired listener generation.
        QVERIFY(!controller.canRetry());
        controller.apply("muteNotification", 0);
        QVERIFY(controller.message().contains("off"));
    }
    void desktopBusContract() {
        // CTest launches this suite on a private bus, never the user's notification service.
        QVERIFY(qEnvironmentVariableIsSet("HEADSET_TEST_PRIVATE_BUS"));
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.isConnected());
        DesktopEndpoint endpoint;
        QVERIFY(bus.registerService("org.freedesktop.Notifications"));
        QVERIFY(
            bus.registerObject("/org/freedesktop/Notifications", &endpoint, QDBusConnection::ExportAllSlots));
        QTemporaryDir directory;
        DeviceDiscovery discovery(
            [](bool) {
                ScanResult s;
                s.devices = {sampleSettingsDevice()};
                return s;
            },
            nullptr, std::chrono::milliseconds::zero());
        Input input;
        int sounds = 0;
        bool audioFails = false;
        NotificationController controller(
            discovery, true, directory.filePath("notifications.ini"),
            [&](const DeviceSnapshot &device) { return input.open(device); }, {}, nullptr,
            [&](const DeviceSnapshot &device, auto done) {
                QCOMPARE(device.key, QString("1-1"));
                ++sounds;
                done(audioFails ? QString("Test audio failure") : QString());
            });
        QTRY_VERIFY(!discovery.busy());
        controller.testNotification(); // Off must remain silent, including explicit test.
        QCOMPARE(sounds, 0);
        QCOMPARE(endpoint.calls, 0);
        controller.apply("muteNotification", 1);
        input.report(0);
        QTRY_COMPARE(endpoint.calls, 1);
        QCOMPARE(sounds, 0);
        QVERIFY(endpoint.lastHints["suppress-sound"].toBool());
        controller.apply("muteNotification", 2);
        input.report(0);
        QTRY_COMPARE(endpoint.calls, 2);
        QCOMPARE(sounds, 1);
        QVERIFY(endpoint.lastHints["suppress-sound"].toBool()); // No doubled desktop chime.
        QVERIFY(!endpoint.lastHints.contains("sound-name"));
        QVERIFY(!controller.canRetry());
        audioFails = true;
        QTRY_VERIFY(controller.canTest());
        controller.testNotification();
        QTRY_VERIFY(controller.message().contains("Test audio failure"));
        QCOMPARE(sounds, 2);
        QCOMPARE(endpoint.calls, 3); // Visual delivery still works when audio fails.
        bus.unregisterObject("/org/freedesktop/Notifications");
        QVERIFY(bus.unregisterService("org.freedesktop.Notifications"));
    }
};
QTEST_GUILESS_MAIN(NotificationsTest)
#include "notifications_test.moc"
