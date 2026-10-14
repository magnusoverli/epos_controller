#include "DeviceAccessSetup.h"
#include "UdevRule.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

DeviceAccessSetup::DeviceAccessSetup(DeviceDiscovery &discovery, SettingsController &settings,
                                     Options options, QObject *parent)
    : QObject(parent), m_discovery(discovery), m_settings(settings), m_options(std::move(options)) {
    if (m_options.stateFile.isEmpty())
        m_options.stateFile = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                              QStringLiteral("/headset-controller/access-setup.ini");
    QSettings state(m_options.stateFile, QSettings::IniFormat);
    m_attempted =
        state.value("automaticAttempted", false).toBool() && state.value("ruleVersion", 0).toInt() >= 2;
    m_settle.setSingleShot(true);
    m_settle.setInterval(m_options.settleMilliseconds);
    connect(&m_settle, &QTimer::timeout, this, [this] {
        if (!m_settings.needsAccess())
            return;
        // Recheck ACLs before asking for privileges: logind may still be handling add.
        m_discovery.refresh(true);
        m_refreshAfterScan = true;
    });
    connect(
        &settings, &SettingsController::changed, this,
        [this] {
            emit changed();
            maybeStartAutomatically();
        },
        Qt::QueuedConnection);
    connect(&discovery, &DeviceDiscovery::scanFinished, this, [this] {
        if (m_refreshAfterScan) {
            m_refreshAfterScan = false;
            if (m_options.automatic && !m_attempted && !m_busy && !m_settings.busy() &&
                m_settings.needsAccess())
                start(true);
            else if (m_attempted || !m_settings.needsAccess())
                m_settings.refresh();
        }
    });
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &DeviceAccessSetup::collectOutput);
    connect(&m_process, &QProcess::finished, this, &DeviceAccessSetup::finish);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_busy = false;
            m_message = tr("Could not start headset access setup: %1").arg(m_process.errorString());
            emit changed();
        }
    });
    QMetaObject::invokeMethod(this, &DeviceAccessSetup::maybeStartAutomatically, Qt::QueuedConnection);
}

DeviceAccessSetup::~DeviceAccessSetup() {
    disconnect(&m_process, nullptr, this, nullptr);
}

void DeviceAccessSetup::maybeStartAutomatically() {
    if (!m_settings.needsAccess()) {
        m_settle.stop();
        return;
    }
    if (m_options.automatic && !m_attempted && !m_busy && !m_settings.busy() && !m_settle.isActive() &&
        !m_refreshAfterScan)
        m_settle.start();
}

void DeviceAccessSetup::enableAccess() {
    if (!m_busy && !m_settings.busy() && m_settings.needsAccess())
        start(false);
}

void DeviceAccessSetup::start(bool automatic) {
    m_attempted = true; // Includes local setup failures: never loop within this launch.
    m_settle.stop();
    auto program = m_options.program;
    auto arguments = m_options.arguments;
    if (program.isEmpty()) {
        program = QStandardPaths::findExecutable(QStringLiteral("pkexec"), {"/usr/bin", "/bin"});
        const QDir executableDirectory(QCoreApplication::applicationDirPath());
        auto helper = executableDirectory.filePath(QStringLiteral("headset-access-helper"));
        if (!QFileInfo(helper).isExecutable())
            helper = executableDirectory.filePath(QString::fromLatin1(DeviceAccess::InstalledHelper));
        if (program.isEmpty() || !QFileInfo(helper).isExecutable()) {
            m_message =
                program.isEmpty()
                    ? tr("Automatic access setup needs pkexec (PolicyKit) and a desktop authentication "
                         "agent. Install your distribution's polkit package, then retry.")
                    : tr("The headset access helper is missing. Rebuild or reinstall the application, then "
                         "retry.");
            emit changed();
            return;
        }
        // Never invoke a shell or pass caller-controlled rules/destination paths to the helper.
        arguments = {QFileInfo(helper).absoluteFilePath()};
    }
    QSettings state(m_options.stateFile, QSettings::IniFormat);
    state.setValue("automaticAttempted", true);
    state.setValue("ruleVersion", 2);
    state.sync();
    if (automatic && state.status() != QSettings::NoError) {
        m_message = tr("Could not save access setup preferences. Use Retry access setup to continue.");
        emit changed();
        return;
    }
    m_output.clear();
    m_busy = true;
    m_message = tr("Setting up headset access. Approve the system's one-time administrator prompt.");
    emit changed();
    m_process.start(program, arguments);
}

void DeviceAccessSetup::collectOutput() {
    m_output += m_process.readAllStandardOutput();
    m_output = m_output.right(4096);
}

void DeviceAccessSetup::finish(int code, QProcess::ExitStatus status) {
    collectOutput();
    m_busy = false;
    const bool normal = status == QProcess::NormalExit;
    if (normal && (code == 0 || code == 3)) {
        m_message =
            code == 0 ? tr("Headset access is set up. If access is still unavailable, reconnect the headset.")
                      : tr("The access rule is installed, but activation was incomplete. %1")
                            .arg(QString::fromLocal8Bit(m_output).trimmed());
        m_refreshAfterScan = true;
        m_discovery.refresh(true);
        emit installed();
    } else if (normal && code == 126) {
        m_message = tr("Headset access setup was cancelled. Use Retry access setup when you are ready.");
    } else {
        m_message = tr("Headset access setup did not complete. Check administrator authorization and your "
                       "desktop authentication agent, then retry. %1")
                        .arg(QString::fromLocal8Bit(m_output).trimmed());
    }
    emit changed();
}
