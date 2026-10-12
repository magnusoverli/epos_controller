#include "BuildInfo.h"
#include "DeviceDiscovery.h"
#include "Diagnostics.h"
#include "DiagnosticsController.h"
#include "SysfsDeviceScanner.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>
#include <memory>
#include <string_view>

namespace {
// Choose the application type before Qt tries to connect to a display server.
bool needsGui(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--diagnostics" || arg == "--list-devices" || arg == "--help" ||
            arg == "-h" || arg == "--version" || arg == "-v") return false;
    }
    return true;
}

int writeJson(const QByteArray &json) {
    const auto size = static_cast<std::size_t>(json.size());
    return std::fwrite(json.constData(), 1, size, stdout) == size && std::fflush(stdout) == 0 ? 0 : 1;
}
}

int main(int argc, char **argv) {
    std::unique_ptr<QCoreApplication> app;
    if (needsGui(argc, argv)) app = std::make_unique<QGuiApplication>(argc, argv);
    else app = std::make_unique<QCoreApplication>(argc, argv);
    app->setApplicationName("Headset Controller");
    app->setApplicationVersion(BuildInfo::Version);

    QCommandLineParser parser;
    parser.setApplicationDescription("Read-only EPOS IMPACT 860T ANC controller");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({
        {"diagnostics", "Print USB/HID diagnostic JSON without a display."},
        {"list-devices", "Print discovered devices as JSON without a display."},
        {"page", "Open page 0 (Home), 1 (Updates), 2 (Settings), or 3 (Help).", "index"},
        {"open-diagnostics", "Open the diagnostics dialog."},
        {"smoke-test", "Exit after UI startup; fail on QML warnings."},
        {"screenshot", "Save a rendered window image and exit.", "file"}
    });
    parser.process(*app);
    bool valid = true;
    const int page = parser.isSet("page") ? parser.value("page").toInt(&valid) : 0;
    if (!valid || page < 0 || page > 3 || !parser.positionalArguments().isEmpty()) {
        std::fputs("Expected --page 0..3 and no positional arguments.\n", stderr);
        return 2;
    }
    const bool cli = parser.isSet("diagnostics") || parser.isSet("list-devices");
    if (cli && (parser.isSet("page") || parser.isSet("open-diagnostics") ||
                parser.isSet("screenshot") || parser.isSet("smoke-test"))) {
        std::fputs("JSON output cannot be combined with GUI options.\n", stderr);
        return 2;
    }
    DeviceDiscovery discovery(SysfsDeviceScanner{}, nullptr,
                              cli ? std::chrono::milliseconds::zero() : DeviceDiscovery::PollInterval);
    if (parser.isSet("diagnostics")) return writeJson(Diagnostics::toJson(discovery.devices()).toUtf8());
    if (parser.isSet("list-devices")) {
        return writeJson(QJsonDocument(QJsonArray::fromVariantList(discovery.devices())).toJson());
    }

    DiagnosticsController diagnostics(discovery);
    bool qmlWarnings = false;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine,
                     [&qmlWarnings] { qmlWarnings = true; });
    engine.setInitialProperties({{"discovery", QVariant::fromValue(&discovery)},
                                 {"diagnosticsController", QVariant::fromValue(&diagnostics)},
                                 {"page", page}, {"deviceSettings", page == 2}});
    engine.load(QUrl("qrc:/qt/qml/HeadsetController/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (parser.isSet("open-diagnostics")) {
        engine.rootObjects().first()->setProperty("page", 3);
        QMetaObject::invokeMethod(engine.rootObjects().first(), "showDiagnostics");
    }
    if (parser.isSet("screenshot")) {
        const QString output = parser.value("screenshot");
        QTimer::singleShot(std::chrono::seconds{1}, &engine, [&engine, &app, output] {
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
            app->exit(window && window->grabWindow().save(output) ? 0 : 2);
        });
    } else if (parser.isSet("smoke-test")) {
        QTimer::singleShot(std::chrono::milliseconds{800}, &engine,
                          [&app, &qmlWarnings] { app->exit(qmlWarnings ? 1 : 0); });
    }
    return app->exec();
}
