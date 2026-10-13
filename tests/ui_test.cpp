#include "DeviceDiscovery.h"
#include "DiagnosticsController.h"
#include "Pages.h"
#include "TestFixtures.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>

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
        QTest::newRow("normal-font") << 14;
        QTest::newRow("larger-font") << 20;
    }

    void navigationSnapshotsAndFocus() {
        QFETCH(int, bodySize);
        QTest::failOnWarning(QRegularExpression(".*"));
        QMutex mutex;
        ScanResult next;
        auto device = sampleDevice();
        device.name = "<b>Device markup is literal</b> " + QString(160, 'x');
        next.devices = {device};
        DeviceDiscovery discovery(
            [&](bool) {
                QMutexLocker lock(&mutex);
                return next;
            },
            nullptr, std::chrono::milliseconds::zero());
        DiagnosticsController controller(discovery);
        QQmlApplicationEngine engine;
        QSignalSpy warnings(&engine, &QQmlApplicationEngine::warnings);
        engine.setInitialProperties({{"discovery", QVariant::fromValue(&discovery)},
                                     {"diagnosticsController", QVariant::fromValue(&controller)},
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
        activate("nav-2");
        QTRY_COMPARE(window->property("page").toInt(), int(Pages::Settings));
        QCOMPARE(list->property("contentY").toDouble(), scrollPosition);
        activate("toggleSettings");
        QTRY_COMPARE(list->property("count").toInt(), 4);
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
