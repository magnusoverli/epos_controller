// Manual integration tool. Never registered with CTest or installed with the application.
#include "SysfsDeviceScanner.h"
#include "settings/LinuxHidTransport.h"
#include "settings/SettingsSession.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>

namespace {
QJsonObject json(const Settings::Values &values) {
    QJsonObject result;
    for (const auto feature : Settings::Features)
        result.insert(QString::fromLatin1(Settings::key(feature)), values.get(feature));
    return result;
}
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.setApplicationDescription("Manual headset setter/readback/restore verification. Requires an idle "
                                     "headset and operator authorization.");
    parser.addOptions(
        {{"device", "USB device key (required).", "key"},
         {"feature", "Settings feature key (required).", "key"},
         {"value", "Supported device-native test value (required).", "raw"},
         {"round-trip", "Explicitly enable a change followed by exact original-value restoration."}});
    parser.process(app);
    bool number = false;
    const auto value = parser.value("value").toInt(&number);
    const auto feature = Settings::featureForKey(parser.value("feature").toStdString());
    if (!parser.positionalArguments().isEmpty() || !parser.isSet("round-trip") ||
        parser.value("device").isEmpty() || !number || value < 0 || value > 65535 || !feature ||
        !Settings::supportedValue(*feature, static_cast<std::uint16_t>(value))) {
        std::fputs("Specify --device KEY --feature KEY --value RAW --round-trip with a supported value.\n",
                   stderr);
        return 2;
    }
    SysfsDeviceScanner scanner;
    const auto scan = scanner(true);
    const DeviceSnapshot *selected = nullptr;
    for (const auto &device : scan.devices)
        if (device.key == parser.value("device"))
            selected = &device;
    if (!selected || scan.stale) {
        std::fputs("Selected device unavailable.\n", stderr);
        return 4;
    }
    auto run = [&](std::optional<Settings::Change> change) -> Settings::Result<Settings::Values> {
        auto transport = Settings::openHidTransport(*selected, {});
        if (!transport)
            return std::unexpected(transport.error());
        Settings::Session session(**transport);
        return change ? session.apply(*change) : session.readAll();
    };
    QJsonObject log{{"startedUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                    {"deviceKey", selected->key},
                    {"usbId", selected->usbId},
                    {"feature", parser.value("feature")},
                    {"testRawValue", value}};
    auto finish = [&](int code) {
        log.insert("finishedUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        const auto bytes = QJsonDocument(log).toJson();
        if (std::fwrite(bytes.constData(), 1, static_cast<std::size_t>(bytes.size()), stdout) !=
                static_cast<std::size_t>(bytes.size()) ||
            std::fflush(stdout) != 0)
            return 1;
        return code;
    };
    const auto before = run(std::nullopt);
    if (!before) {
        log.insert("error", QString::fromStdString(before.error().message));
        return finish(4);
    }
    log.insert("before", json(*before));
    if (!Settings::supportedValue(*feature, before->get(*feature)) || before->get(*feature) == value) {
        log.insert("error",
                   "Original value cannot be restored using a verified setter, or test value is unchanged.");
        return finish(4);
    }
    const auto changed = run(Settings::Change{*feature, static_cast<std::uint16_t>(value), *before});
    if (!changed) {
        log.insert("error", QString::fromStdString(changed.error().message));
        log.insert("writeMayHaveApplied", changed.error().writeMayHaveApplied);
        log.insert("restoration",
                   "Not attempted after failed change; stop and inspect live values before recovery.");
        return finish(4);
    }
    log.insert("changed", json(*changed));
    const auto restored = run(Settings::Change{*feature, before->get(*feature), *changed});
    if (!restored) {
        log.insert("error", QString::fromStdString(restored.error().message));
        log.insert("restoration", "Unverified; stop further experiments and inspect live values.");
        return finish(4);
    }
    log.insert("restored", json(*restored));
    log.insert("exactRestoration", *restored == *before);
    return finish(*restored == *before ? 0 : 4);
}
