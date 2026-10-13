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
        QCoreApplication::translate("CommandLine", "Read-only EPOS IMPACT 860T ANC controller"));
    parser.addOptions({{{"h", "help"}, "Display help without requiring a display server."},
                       {"help-all", "Display help including supported Qt options."},
                       {{"v", "version"}, "Display application version."},
                       {"diagnostics", "Print USB/HID diagnostic JSON without a display."},
                       {"list-devices", "Print discovered devices as JSON without a display."},
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
