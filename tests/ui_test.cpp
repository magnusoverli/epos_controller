#include "DeviceAccessSetup.h"
#include "DeviceDiscovery.h"
#include "DiagnosticsController.h"
#include "FakeFirmwareNetwork.h"
#include "FakeSettingsTransport.h"
#include "FirmwareController.h"
#include "NotificationController.h"
#include "Pages.h"
#include "SettingsController.h"
#include "TestFixtures.h"
#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QMutex>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <atomic>
#include <functional>
#include <utility>

namespace {
QQuickItem *findVisual(QQuickItem *root, const QString &name) {
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *found = findVisual(child, name))
            return found;
    return nullptr;
}
} // namespace

class UiTest : public QObject {
    Q_OBJECT
  private slots:
    void navigationSnapshotsAndFocus_data() {
        QTest::addColumn<int>("bodySize");
        QTest::addColumn<bool>("automaticReads");
        QTest::newRow("normal-font") << 14 << true;
        QTest::newRow("larger-font") << 20 << true;
        QTest::newRow("read-only-startup") << 14 << false;
    }

    void navigationSnapshotsAndFocus() {
        QFETCH(int, bodySize);
        QFETCH(bool, automaticReads);
        QTest::failOnWarning(QRegularExpression(".*"));
        QMutex mutex;
        ScanResult next;
        auto device = sampleSettingsDevice();
        device.name = "<b>Device markup is literal</b> " + QString(160, 'x');
        next.devices = {device};
        DeviceDiscovery discovery(
            [&](bool) {
                QMutexLocker lock(&mutex);
                return next;
            },
            nullptr, std::chrono::milliseconds::zero());
        DiagnosticsController controller(discovery);
        auto hardware = std::make_shared<FakeSettingsDevice>();
        const auto writeCount = [&] {
            std::lock_guard lock(hardware->mutex);
            return hardware->writes;
        };
        QSemaphore entered, resume;
        std::atomic<bool> holdNext{false};
        SettingsController settingsController(
            discovery,
            [&, hardware](const DeviceSnapshot &,
                          std::stop_token) -> Settings::Result<std::unique_ptr<Settings::ReportTransport>> {
                if (holdNext.exchange(false)) {
                    entered.release();
                    resume.acquire();
                }
                return std::make_unique<FakeSettingsTransport>(hardware);
            });
        const auto releaseOnExit = qScopeGuard([&] { resume.release(); });
        FakeNetwork firmwareNetwork;
        FirmwareController firmwareController(settingsController, true, &firmwareNetwork);
        QTemporaryDir accessState;
        QVERIFY(accessState.isValid());
        DeviceAccessSetup accessSetup(discovery, settingsController,
                                      {.automatic = false, .stateFile = accessState.filePath("access.ini")});
        QTemporaryDir notificationPreferences;
        QVERIFY(notificationPreferences.isValid());
        NotificationController notifications(
            discovery, true, notificationPreferences.filePath("notifications.ini"),
            [](const DeviceSnapshot &) -> Settings::Result<int> {
                return std::unexpected(
                    Settings::Error{Settings::ErrorCode::AccessDenied, "Fake event access"});
            },
            [](bool, auto done) { done({}); });
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlApplicationEngine::warnings);
        engine.setInitialProperties({{"discovery", QVariant::fromValue(&discovery)},
                                     {"diagnosticsController", QVariant::fromValue(&controller)},
                                     {"settingsController", QVariant::fromValue(&settingsController)},
                                     {"firmwareController", QVariant::fromValue(&firmwareController)},
                                     {"notificationController", QVariant::fromValue(&notifications)},
                                     {"accessSetup", QVariant::fromValue(&accessSetup)},
                                     {"automaticSettingsReads", automaticReads},
                                     {"width", 960},
                                     {"height", 650}});
        engine.loadFromModule("HeadsetController", "Main");
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        auto font = window->property("font").value<QFont>();
        font.setPixelSize(bodySize);
        window->setProperty("font", font);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTRY_VERIFY(!discovery.busy());
        QCOMPARE(hardware->sends, 0); // Home/discovery does not open a settings transport.
        auto find = [&](const char *objectName) -> QObject * {
            const auto name = QString::fromLatin1(objectName);
            if (auto *object = window->findChild<QObject *>(name))
                return object;
            return findVisual(window->contentItem(), name);
        };
        auto button = [&](const char *name) { return qobject_cast<QQuickItem *>(find(name)); };
        QVERIFY(find("homeLoader")->property("active").toBool());
        QVERIFY(!find("settingsLoader")->property("active").toBool());
        QVERIFY(!find("updatesLoader")->property("active").toBool());
        auto *name = button("deviceName");
        QVERIFY(name);
        QCOMPARE(name->property("text").toString(), device.name);
        QCOMPARE(name->property("textFormat").toInt(), 0); // PlainText
        QVERIFY(name->width() <= 410);
        auto *settings = button("deviceSettingsButton");
        QVERIFY(settings);
        settings->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(settings->property("visualFocus").toBool());
        auto *background = settings->findChild<QObject *>("buttonBackground");
        QVERIFY(background);
        QCOMPARE(QQmlProperty(background, "border.width").read().toInt(), 3);
        // Both focus rendering and activation must work with keyboard input.
        QVERIFY(!window->grabWindow().isNull());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_COMPARE(window->property("page").toInt(), int(Pages::Settings));
        QVERIFY(find("settingsLoader")->property("active").toBool());
        auto *list = button("settingsList");
        QVERIFY(list);
        QCOMPARE(list->property("count").toInt(), 25);
        auto *readSettings = button("readSettings");
        QVERIFY(readSettings);
        if (!automaticReads) {
            QVERIFY(!settingsController.active());
            QCOMPARE(hardware->sends, 0); // Screenshots/smoke startup stay descriptor-only.
            readSettings->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(window, Qt::Key_Space);
        }
        QTRY_VERIFY(settingsController.ready());
        QCOMPARE(readSettings->property("text").toString(), QString("Refresh"));
        QCOMPARE(hardware->sends, int(Settings::Features.size())); // Opening settings loads every control.
        QTRY_VERIFY(list->height() > 0);
        QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
        QVERIFY(!button("testNotification"));
        const auto captureDirectory = qEnvironmentVariable("HEADSET_UI_CAPTURE_DIR");
        for (const auto *name : {"settings-section-device-audio", "settings-section-device-sounds",
                                 "settings-section-device-device"}) {
            QTRY_VERIFY(button(name));
            QVERIFY(!button(name)->property("checked").toBool());
        }
        QVERIFY(!button("setting-sidetone") || !button("setting-sidetone")->isVisible());
        if (!captureDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(captureDirectory));
            QVERIFY(window->grabWindow().save(captureDirectory + "/collapsed-" +
                                              QString::fromLatin1(QTest::currentDataTag()) + ".png"));
        }
        auto *audioSection = button("settings-section-device-audio");
        QTest::mouseClick(window, Qt::LeftButton, {},
                          audioSection->mapToScene(audioSection->boundingRect().center()).toPoint());
        QTRY_VERIFY(audioSection->property("checked").toBool());
        QTRY_VERIFY(button("setting-sidetone") && button("setting-sidetone")->isVisible());
        audioSection->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!audioSection->property("checked").toBool());
        QTRY_VERIFY(!button("setting-sidetone") || !button("setting-sidetone")->isVisible());
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(audioSection->property("checked").toBool());
        for (const auto &[index, name] : {std::pair{12, "settings-section-device-sounds"},
                                          std::pair{21, "settings-section-device-device"}}) {
            QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
            QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, index), Q_ARG(int, 0)));
            QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
            QTRY_VERIFY(button(name));
            button(name)->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(window, Qt::Key_Space);
            QTRY_VERIFY(button(name)->property("checked").toBool());
        }
        QCOMPARE(hardware->writes, 0); // Expanding/collapsing sections never changes settings.
        QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
        QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, 4), Q_ARG(int, 0)));
        QTRY_VERIFY(button("setting-sidetone"));
        auto *sidetone = button("setting-sidetone");
        QVERIFY(sidetone);
        QCOMPARE(button("sidetoneReadback")->property("text").toString(), QString("32%"));
        QCOMPARE(sidetone->property("value").toInt(), 32);
        QCOMPARE(sidetone->property("from").toInt(), 0);
        QCOMPARE(sidetone->property("to").toInt(), 100);
        QVERIFY(!button("applySidetone"));
        QCOMPARE(sidetone->property("focusPolicy").toInt(), int(Qt::NoFocus));
        auto sliderPoint = [&](int percent) {
            auto *handle = qobject_cast<QQuickItem *>(sidetone->property("handle").value<QObject *>());
            const auto handleWidth = handle ? handle->width() : 0;
            const auto x = sidetone->property("leftPadding").toDouble() + handleWidth / 2 +
                           (sidetone->property("availableWidth").toDouble() - handleWidth) * percent / 100;
            return sidetone->mapToScene(QPointF(x, sidetone->height() / 2)).toPoint();
        };
        QTest::mousePress(window, Qt::LeftButton, {}, sliderPoint(32));
        QTest::mouseMove(window, sliderPoint(60));
        QTest::mouseMove(window, sliderPoint(33));
        QCOMPARE(hardware->writes, 0); // Dragging does not flood the device with setters.
        QTest::mouseRelease(window, Qt::LeftButton, {}, sliderPoint(33));
        QTRY_COMPARE(writeCount(), 1);
        QTRY_VERIFY(!settingsController.busy());
        QTRY_COMPARE(button("sidetoneReadback")->property("text").toString(), QString("33%"));
        QCOMPARE(sidetone->property("value").toInt(), 33);
        QCOMPARE(hardware->writes, 1);
        // Unrelated in-flight writes must not disable controls or overwrite a draft.
        QTest::mousePress(window, Qt::LeftButton, {}, sliderPoint(33));
        QTest::mouseMove(window, sliderPoint(50));
        const auto draft = sidetone->property("value").toInt();
        QVERIFY(draft > 33);
        holdNext = true;
        settingsController.apply("ancMode", 0);
        QVERIFY(entered.tryAcquire(1, 2000));
        QVERIFY(settingsController.busy());
        QVERIFY(sidetone->isEnabled());
        QVERIFY(button("setting-audioLimiter")->isEnabled());
        QCOMPARE(readSettings->property("text").toString(), QString("Refresh"));
        QCOMPARE(sidetone->property("value").toInt(), draft);
        settingsController.apply("audioLimiter", 2);
        QCOMPARE(button("setting-audioLimiter")->property("displayText").toString(), QString("AU limiter"));
        resume.release();
        QTRY_VERIFY(!settingsController.busy());
        QCOMPARE(sidetone->property("value").toInt(), draft);
        auto *percentEditor = sidetone->parentItem()->parentItem();
        QSignalSpy applySignal(percentEditor, SIGNAL(applyRequested(int)));
        QTest::mouseRelease(window, Qt::LeftButton, {}, sliderPoint(draft));
        QCOMPARE(applySignal.size(), 1);
        QTRY_COMPARE(writeCount(), 4);
        QTRY_VERIFY(!settingsController.busy());
        QCOMPARE(Settings::sidetonePercent(hardware->values.sidetone), std::optional(draft));
        settingsController.applySidetonePercent(33);
        settingsController.apply("ancMode", 1);
        settingsController.apply("audioLimiter", 0);
        QTRY_VERIFY(!settingsController.busy());
        QCOMPARE(sidetone->property("value").toInt(), 33);
        QCOMPARE(hardware->writes, 7);
        // An explicit refresh invalidates an in-progress drag; release must not replay it.
        QTest::mousePress(window, Qt::LeftButton, {}, sliderPoint(33));
        QTest::mouseMove(window, sliderPoint(70));
        settingsController.refresh();
        QTRY_VERIFY(!settingsController.busy());
        QTest::mouseRelease(window, Qt::LeftButton, {}, sliderPoint(70));
        QTRY_COMPARE(sidetone->property("value").toInt(), 33);
        QCOMPARE(hardware->writes, 7);
        if (!captureDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(captureDirectory));
            QVERIFY(window->grabWindow().save(captureDirectory + "/settings-" +
                                              QString::fromLatin1(QTest::currentDataTag()) + ".png"));
        }
        // Exercise non-boolean native encodings through the actual dropdown UI.
        // Virtualized delegates have bounded lifetimes. Interact with rendered,
        // visible items and reacquire them after readback.
        auto visibleSetting = [&](const char *name) {
            std::function<QQuickItem *(QQuickItem *)> visit = [&](QQuickItem *item) -> QQuickItem * {
                if (item->objectName() == QString::fromLatin1(name) && item->isVisible() &&
                    list->boundingRect().contains(item->mapToItem(list, item->boundingRect().center())))
                    return item;
                for (auto *child : item->childItems())
                    if (auto *found = visit(child))
                        return found;
                return nullptr;
            };
            return visit(list);
        };
        struct Preference {
            int index;
            const char *objectName;
            const char *label;
            Qt::Key key;
        };
        constexpr Preference preferences[]{{15, "setting-noPromptsDuringCalls", "On", Qt::Key_End},
                                           {22, "setting-buttonStatus", "Disabled", Qt::Key_End},
                                           {5, "setting-audioLimiter", "AU limiter", Qt::Key_End},
                                           {7, "setting-answerOnWear", "On", Qt::Key_End},
                                           {9, "setting-muteOnRemoval", "On", Qt::Key_End},
                                           {23, "setting-smartPause", "Off", Qt::Key_Home},
                                           {11, "setting-autoPause", "On", Qt::Key_End},
                                           {13, "setting-prompts", "Tone prompts", Qt::Key_Up},
                                           {14, "setting-promptLanguage", "Korean", Qt::Key_End},
                                           {18, "setting-muteNotification", "Visual and audio", Qt::Key_End},
                                           {19, "setting-muteInterval", "10 minutes", Qt::Key_End}};
        for (const auto &[index, objectName, label, key] : preferences) {
            QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, index), Q_ARG(int, 0)));
            QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
            QSignalSpy rendered(window, &QQuickWindow::frameSwapped);
            window->requestUpdate();
            QTRY_VERIFY(!rendered.isEmpty());
            QTRY_VERIFY2(visibleSetting(objectName), objectName);
            QPointer<QQuickItem> control = visibleSetting(objectName);
            QVERIFY(control->isEnabled());
            control->forceActiveFocus(Qt::TabFocusReason);
            QTRY_VERIFY(control->hasActiveFocus());
            QTest::keyClick(window, Qt::Key_Space);
            QPointer<QObject> popup = control->property("popup").value<QObject *>();
            QVERIFY(popup);
            QTRY_VERIFY(popup && popup->property("opened").toBool());
            QVERIFY(control);
            QCOMPARE(control->objectName(), QString::fromLatin1(objectName));
            QTest::keyClick(window, key);
            QTest::keyClick(window, Qt::Key_Return);
            // Finish the popup's exit transition before scrolling/removing its
            // delegate; a closing popup can otherwise restore focus to the old row.
            QTRY_VERIFY(!popup || !popup->property("visible").toBool());
            QTRY_VERIFY(!settingsController.busy());
            QVERIFY(settingsController.ready());
            QTRY_VERIFY2(visibleSetting(objectName) &&
                             visibleSetting(objectName)->property("displayText").toString() ==
                                 QString::fromLatin1(label),
                         objectName);
        }
        QCOMPARE(hardware->values.noPromptsDuringCalls, 0);
        QCOMPARE(hardware->values.buttonStatus, 0xffef);
        QCOMPARE(hardware->values.audioLimiter, 2);
        QCOMPARE(hardware->values.answerOnWear, 1);
        QCOMPARE(hardware->values.muteOnRemoval, 1);
        QCOMPARE(hardware->values.smartPause, 0);
        QCOMPARE(hardware->values.autoPause, 1);
        QCOMPARE(hardware->values.prompts, 2);
        QCOMPARE(hardware->values.promptLanguage, 7);
        QCOMPARE(hardware->writes, 16);
        QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, 24), Q_ARG(int, 0)));
        QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
        QTRY_VERIFY(visibleSetting("factoryReset"));
        visibleSetting("factoryReset")->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        auto *resetDialog = find("factoryResetDialog");
        QVERIFY(resetDialog);
        QTRY_VERIFY(resetDialog->property("opened").toBool());
        QTRY_VERIFY(button("cancelFactoryReset")->hasActiveFocus());
        QCOMPARE(hardware->resets, 0); // Opening the confirmation performs no device action.
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!resetDialog->property("opened").toBool());
        QCOMPARE(hardware->resets, 0);
        visibleSetting("factoryReset")->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(resetDialog->property("opened").toBool());
        {
            QMutexLocker lock(&mutex);
            next.devices.clear();
        }
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QTRY_VERIFY(!resetDialog->property("opened").toBool());
        QCOMPARE(hardware->resets, 0); // Disconnect invalidates the pending confirmation.
        {
            QMutexLocker lock(&mutex);
            next.devices = {device};
        }
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        settingsController.refresh();
        QTRY_VERIFY(settingsController.ready());
        QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtIndex", Q_ARG(int, 24), Q_ARG(int, 0)));
        QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
        QTRY_VERIFY(visibleSetting("factoryReset"));
        visibleSetting("factoryReset")->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(resetDialog->property("opened").toBool());
        button("confirmFactoryReset")->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(!resetDialog->property("opened").toBool());
        QTRY_VERIFY(settingsController.ready());
        QCOMPARE(hardware->resets, 1);
        QCOMPARE(hardware->values, hardware->valuesAfterReset);
        QTRY_VERIFY(list->height() > 0);
        QTRY_VERIFY(list->property("contentHeight").toDouble() > list->height());
        QVERIFY(QMetaObject::invokeMethod(list, "forceLayout"));
        QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtEnd"));
        QTRY_VERIFY(list->property("contentY").toDouble() > 0);
        const auto scrollPosition = list->property("contentY").toDouble();
        auto activate = [&](const char *object) {
            auto *item = button(object);
            check(item != nullptr, object);
            item->forceActiveFocus(Qt::TabFocusReason);
            QTest::keyClick(window, Qt::Key_Space);
        };
        activate("nav-1");
        QTRY_COMPARE(window->property("page").toInt(), int(Pages::Updates));
        QVERIFY(!find("loadFirmwareOffer"));
        QVERIFY(!find("inspectFirmwarePackage"));
        QCOMPARE(firmwareNetwork.requests,
                 0); // Waiting for real firmware identity, not guessing from bcdDevice.
        settingsController.setIdentityReadsEnabled(true);
        QTRY_COMPARE(firmwareNetwork.requests, 1);
        auto installableRelease = FakeNetwork::release();
        installableRelease["HashOfZipFile"] =
            "1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd";
        firmwareNetwork.catalog({installableRelease});
        QTRY_COMPARE(button("firmwareVersion")->property("text").toString(),
                     QString("Installed firmware: 1.5.64"));
        QCOMPARE(button("latestFirmwareVersion")->property("text").toString(),
                 QString("Latest firmware: 1.5.619"));
        QVERIFY(firmwareController.updateAvailable());
        QTRY_VERIFY(firmwareController.canInstall());
        activate("installFirmwareUpdate");
        auto *installDialog = find("firmwareInstallDialog");
        QVERIFY(installDialog);
        QTRY_VERIFY(installDialog->property("opened").toBool());
        QCOMPARE(firmwareNetwork.requests, 1); // Confirmation does not download or write.
        activate("cancelFirmwareInstall");
        QTRY_VERIFY(!installDialog->property("opened").toBool());
        QVERIFY(!firmwareController.installing());
        QVERIFY(button("globalFirmwareUpdateNotice")->isVisible());
        activate("nav-0");
        QTRY_VERIFY(button("firmwareUpdateNotice")->isVisible());
        QCOMPARE(button("homeFirmwareVersion")->property("text").toString(), QString("Firmware: 1.5.64"));
        activate("firmwareUpdateNotice");
        QTRY_COMPARE(window->property("page").toInt(), int(Pages::Updates));
        activate("checkFirmwareUpdates");
        QCOMPARE(firmwareNetwork.requests, 2);
        firmwareNetwork.catalog({FakeNetwork::release()});
        QVERIFY(!window->grabWindow().isNull());
        activate("nav-2");
        QTRY_COMPARE(window->property("page").toInt(), int(Pages::Settings));
        QTRY_VERIFY(settingsController.ready());
        QCOMPARE(button("settingsFirmwareVersion")->property("text").toString(), QString("Firmware: 1.5.64"));
        QCOMPARE(list->property("contentY").toDouble(), scrollPosition);
        activate("toggleSettings");
        QTRY_COMPARE(list->property("count").toInt(), 4);
        QTRY_VERIFY(button("settings-section-application-softphone"));
        QVERIFY(!button("settings-section-application-softphone")->property("checked").toBool());
        QVERIFY(!button("settings-section-application-language")->property("checked").toBool());
        activate("settings-section-application-softphone");
        QVERIFY(button("settings-section-application-softphone")->property("checked").toBool());
        activate("toggleSettings");
        QTRY_COMPARE(list->property("count").toInt(), 25);
        QVERIFY(QMetaObject::invokeMethod(list, "positionViewAtBeginning"));
        QTRY_VERIFY(button("settings-section-device-audio"));
        QVERIFY(button("settings-section-device-audio")->property("checked").toBool());
        activate("toggleSettings");
        QTRY_COMPARE(list->property("count").toInt(), 4);
        QTRY_VERIFY(button("settings-section-application-softphone"));
        QVERIFY(button("settings-section-application-softphone")->property("checked").toBool());
        activate("nav-3");
        QTRY_COMPARE(window->property("page").toInt(), int(Pages::Help));
        auto *inspect = button("inspectInterfaces");
        QVERIFY(inspect);
        inspect->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(QMetaObject::invokeMethod(window, "showDiagnostics"));
        auto *dialog = find("diagnosticsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(!controller.busy());
        const auto displayed = controller.snapshot();
        QVERIFY(displayed.contains("scanStatus"));
        {
            QMutexLocker lock(&mutex);
            next.devices.clear();
        }
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(!discovery.selectedDevice()->connected());
        QVERIFY(!settingsController.ready());
        QVERIFY(firmwareController.latestVersion().isEmpty());
        QVERIFY(!firmwareController.updateAvailable());
        QVERIFY(!button("globalFirmwareUpdateNotice")->isVisible());
        QCOMPARE(controller.snapshot(), displayed); // Live polls do not silently replace exported text.
        activate("copyDiagnostics");
        QCOMPARE(QGuiApplication::clipboard()->text(), displayed);
        activate("refreshDiagnostics");
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.snapshot() != displayed);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QTRY_VERIFY(inspect->hasActiveFocus());
        activate("nav-0");
        QVERIFY(button("refreshDevices")->isVisible());
        {
            QMutexLocker lock(&mutex);
            next.devices = {sampleDevice("2-1", "2-1#2"), sampleDevice("3-1", "3-1#3")};
            next.devices.first().name = "<i>Select this literal name</i>";
        }
        discovery.refresh(true);
        QTRY_VERIFY(!discovery.busy());
        QVERIFY(!discovery.selectedDevice()->connected());
        auto *selector = button("deviceSelector");
        QVERIFY(selector);
        QVERIFY(selector->isVisible());
        selector->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        auto *popup = selector->property("popup").value<QObject *>();
        QVERIFY(popup);
        QTRY_VERIFY(popup->property("opened").toBool());
        QTest::keyClick(window, Qt::Key_Home);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(discovery.selectedDevice()->connectionId(), QString("2-1#2"));
        QTRY_VERIFY(!popup->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(settings->hasActiveFocus());
        QCOMPARE(warnings.count(), 0);
    }
};

QTEST_MAIN(UiTest)
#include "ui_test.moc"
