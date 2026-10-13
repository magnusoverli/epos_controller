#include "BuildInfo.h"
#include "CommandLine.h"
#include "DeviceDiscovery.h"
#include "DeviceMonitor.h"
#include "Diagnostics.h"
#include "DiagnosticsController.h"
#include "SysfsDeviceScanner.h"
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>
#include <memory>

namespace {
int writeOutput(const QByteArray &bytes) {
    const auto size = static_cast<std::size_t>(bytes.size());
    return std::fwrite(bytes.constData(), 1, size, stdout) == size && std::fflush(stdout) == 0 ? 0 : 1;
}
} // namespace

int main(int argc, char **argv) {
    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments.append(QString::fromLocal8Bit(argv[i]));
    const auto parsed = CommandLine::parse(arguments);
    if (!parsed) {
        std::fprintf(stderr, "%s\n", qPrintable(parsed.error()));
        return 2;
    }
    const auto &options = *parsed;
    // Qt only sees its own options. A screenshot filename such as "--help" remains a filename.
    std::vector<QByteArray> storage;
    for (const auto &argument : options.qtArguments)
        storage.push_back(argument.toLocal8Bit());
    std::vector<char *> qtArgv;
    for (auto &argument : storage)
        qtArgv.push_back(argument.data());
    qtArgv.push_back(nullptr);
    int qtArgc = static_cast<int>(storage.size());
    std::unique_ptr<QCoreApplication> app;
    if (options.mode == LaunchMode::Gui)
        app = std::make_unique<QGuiApplication>(qtArgc, qtArgv.data());
    else
        app = std::make_unique<QCoreApplication>(qtArgc, qtArgv.data());
    app->setApplicationName(BuildInfo::Name);
    app->setApplicationVersion(BuildInfo::Version);
    if (options.mode == LaunchMode::Help)
        return writeOutput(CommandLine::helpText().toUtf8());
    if (options.mode == LaunchMode::Version)
        return writeOutput((app->applicationName() + ' ' + app->applicationVersion() + '\n').toUtf8());
    if (options.mode != LaunchMode::Gui) {
        SysfsDeviceScanner scanner;
        const auto snapshot = scanner(true);
        const auto output =
            options.mode == LaunchMode::Diagnostics
                ? Diagnostics::toJson(snapshot)
                : QJsonDocument(QJsonArray::fromVariantList(deviceVariants(snapshot.devices))).toJson();
        const int written = writeOutput(output);
        return written != 0 ? written : snapshot.status == ScanStatus::Unavailable ? 3 : 0;
    }

    DeviceMonitor monitor;
    DeviceDiscovery discovery(SysfsDeviceScanner{}, nullptr,
                              monitor.available() ? std::chrono::seconds{30} : DeviceDiscovery::PollInterval);
    QObject::connect(&monitor, &DeviceMonitor::topologyChanged, &discovery, [&] { discovery.refresh(true); });
    DiagnosticsController diagnostics(discovery);
    bool qmlWarnings = false;
    bool rendered = false;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &engine, [&] { qmlWarnings = true; });
    engine.setInitialProperties({{"discovery", QVariant::fromValue(&discovery)},
                                 {"diagnosticsController", QVariant::fromValue(&diagnostics)},
                                 {"page", static_cast<int>(options.page)},
                                 {"deviceSettings", options.page == Pages::Settings}});
    engine.loadFromModule("HeadsetController", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    QObject::connect(
        window, &QQuickWindow::frameSwapped, &engine, [&] { rendered = true; }, Qt::QueuedConnection);
    if (options.openDiagnostics) {
        window->setProperty("page", Pages::Help);
        QMetaObject::invokeMethod(window, "showDiagnostics");
    }
    QTimer ready;
    if (options.smokeTest || !options.screenshot.isEmpty()) {
        QObject::connect(&ready, &QTimer::timeout, &engine, [&] {
            if (!rendered || discovery.busy() || diagnostics.busy())
                return;
            ready.stop();
            const bool saved = options.screenshot.isEmpty() || window->grabWindow().save(options.screenshot);
            app->exit(!saved ? 2 : options.smokeTest && qmlWarnings ? 1 : 0);
        });
        ready.start(std::chrono::milliseconds{20});
        QTimer::singleShot(std::chrono::seconds{10}, &engine, [&] {
            std::fputs("Timed out waiting for rendered UI and discovery.\n", stderr);
            app->exit(1);
        });
        window->requestUpdate();
    }
    return app->exec();
}
