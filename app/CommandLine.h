#pragma once
#include "Pages.h"
#include <QStringList>
#include <expected>

enum class LaunchMode { Gui, Diagnostics, ListDevices, Help, Version };
struct LaunchOptions {
    LaunchMode mode = LaunchMode::Gui;
    Pages::Page page = Pages::Home;
    bool openDiagnostics = false;
    bool smokeTest = false;
    QString screenshot;
    QStringList qtArguments;
};

namespace CommandLine {
[[nodiscard]] std::expected<LaunchOptions, QString> parse(const QStringList &arguments);
[[nodiscard]] QString helpText();
} // namespace CommandLine
