#include "CommandLine.h"
#include <QCommandLineParser>
#include <QCoreApplication>

namespace {
struct QtOption {
    const char *name;
    bool takesValue;
};
// Forward only actual Qt options, never application-option values, to QGuiApplication.
constexpr QtOption qtOptions[] = {{"platform", true},
                                  {"platformpluginpath", true},
                                  {"platformtheme", true},
                                  {"plugin", true},
                                  {"display", true},
                                  {"geometry", true},
                                  {"font", true},
                                  {"name", true},
                                  {"title", true},
                                  {"visual", true},
                                  {"ncols", true},
                                  {"session", true},
                                  {"qwindowgeometry", true},
                                  {"qwindowicon", true},
                                  {"qwindowtitle", true},
                                  {"qmljsdebugger", true},
                                  {"reverse", false},
                                  {"nograb", false},
                                  {"dograb", false},
                                  {"sync", false}};

void configure(QCommandLineParser &parser) {
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription(
        QCoreApplication::translate("CommandLine", "EPOS IMPACT 860T ANC controller for Linux"));
    parser.addOptions(
        {{{"h", "help"}, "Display help without requiring a display server."},
         {"help-all", "Display help including supported Qt options."},
         {{"v", "version"}, "Display application version."},
         {"diagnostics", "Print USB/HID diagnostic JSON without a display."},
         {"list-devices", "Print discovered devices as JSON without a display."},
         {"read-settings", "Read live headset settings over HID and print JSON (requires device access)."},
         {"read-firmware", "Read the live headset firmware version over HID and print JSON."},
         {"inspect-firmware", "Inspect a firmware ZIP offline; requires --firmware-offer. Never installs.",
          "zip"},
         {"firmware-offer", "Exported single firmware offer JSON for offline integrity checks.", "json"},
         {"installed-version", "Optional version for offline numeric comparison, e.g. 1.5.64.", "version"},
         {"device", "USB device key for --read-settings; required when multiple headsets are present.",
          "key"},
         {"page", "Open page 0 (Home), 1 (Updates), 2 (Settings), or 3 (Help).", "index"},
         {"open-diagnostics", "Open the diagnostics dialog."},
         {"smoke-test", "Exit after a rendered frame and discovery; fail on QML warnings."},
         {"screenshot", "Save a rendered window image and exit.", "file"}});
    for (const auto &option : qtOptions) {
        parser.addOption(QCommandLineOption(QString::fromLatin1(option.name),
                                            "Qt GUI option (forwarded to QGuiApplication).",
                                            option.takesValue ? "value" : QString()));
    }
}
} // namespace

std::expected<LaunchOptions, QString> CommandLine::parse(const QStringList &arguments) {
    QCommandLineParser parser;
    configure(parser);
    if (!parser.parse(arguments))
        return std::unexpected(parser.errorText());
    LaunchOptions options;
    options.qtArguments.append(arguments.value(0));
    if (parser.isSet("help") || parser.isSet("help-all")) {
        options.mode = LaunchMode::Help;
        return options;
    }
    if (parser.isSet("version")) {
        options.mode = LaunchMode::Version;
        return options;
    }
    if (!parser.positionalArguments().isEmpty())
        return std::unexpected(QStringLiteral("No positional arguments are accepted."));
    bool valid = true;
    const int page = parser.isSet("page") ? parser.value("page").toInt(&valid) : Pages::Home;
    if (!valid || page < Pages::Home || page >= Pages::Count)
        return std::unexpected(QStringLiteral("Expected --page 0..3."));
    options.page = static_cast<Pages::Page>(page);
    options.openDiagnostics = parser.isSet("open-diagnostics");
    options.smokeTest = parser.isSet("smoke-test");
    options.screenshot = parser.value("screenshot");
    if (parser.isSet("screenshot") && options.screenshot.isEmpty())
        return std::unexpected(QStringLiteral("Screenshot filename cannot be empty."));
    if (parser.isSet("diagnostics") && parser.isSet("list-devices"))
        return std::unexpected(QStringLiteral("Choose either --diagnostics or --list-devices."));
    if (parser.isSet("diagnostics"))
        options.mode = LaunchMode::Diagnostics;
    if (parser.isSet("list-devices"))
        options.mode = LaunchMode::ListDevices;
    if (parser.isSet("read-settings")) {
        if (options.mode != LaunchMode::Gui)
            return std::unexpected(QStringLiteral("Choose only one JSON command."));
        options.mode = LaunchMode::ReadSettings;
    }
    if (parser.isSet("read-firmware")) {
        if (options.mode != LaunchMode::Gui)
            return std::unexpected(QStringLiteral("Choose only one JSON command."));
        options.mode = LaunchMode::ReadFirmware;
    }
    if (parser.isSet("device")) {
        if (options.mode != LaunchMode::ReadSettings && options.mode != LaunchMode::ReadFirmware)
            return std::unexpected(QStringLiteral("--device requires --read-settings or --read-firmware."));
        options.deviceKey = parser.value("device");
        if (options.deviceKey.isEmpty())
            return std::unexpected(QStringLiteral("Device key cannot be empty."));
    }
    if (parser.isSet("inspect-firmware")) {
        if (options.mode != LaunchMode::Gui || parser.isSet("device"))
            return std::unexpected(QStringLiteral("Choose only one JSON command."));
        options.mode = LaunchMode::InspectFirmware;
        options.firmwarePackage = parser.value("inspect-firmware");
        options.firmwareOffer = parser.value("firmware-offer");
        options.installedVersion = parser.value("installed-version");
        if (options.firmwarePackage.isEmpty() || options.firmwareOffer.isEmpty())
            return std::unexpected(
                QStringLiteral("--inspect-firmware requires nonempty ZIP and --firmware-offer paths."));
        if (parser.isSet("installed-version") && options.installedVersion.isEmpty())
            return std::unexpected(QStringLiteral("Installed version cannot be empty."));
    } else if (parser.isSet("firmware-offer") || parser.isSet("installed-version")) {
        return std::unexpected(
            QStringLiteral("--firmware-offer and --installed-version require --inspect-firmware."));
    }
    bool guiOption =
        parser.isSet("page") || options.openDiagnostics || options.smokeTest || parser.isSet("screenshot");
    for (const auto &option : qtOptions) {
        const auto name = QString::fromLatin1(option.name);
        if (!parser.isSet(name))
            continue;
        guiOption = true;
        if (option.takesValue) {
            for (const auto &value : parser.values(name))
                options.qtArguments << '-' + name << value;
        } else
            options.qtArguments << '-' + name;
    }
    if (options.mode != LaunchMode::Gui && guiOption)
        return std::unexpected(QStringLiteral("JSON output cannot be combined with GUI options."));
    return options;
}

QString CommandLine::helpText() {
    QCommandLineParser parser;
    configure(parser);
    return parser.helpText();
}
