#include "SettingsController.h"
#include "SettingsPresentation.h"
#include "settings/SettingsSession.h"
#include <QTimer>

SettingsController::SettingsController(DeviceDiscovery &discovery, Settings::TransportFactory factory,
                                       QObject *parent)
    : QObject(parent), m_discovery(discovery), m_factory(std::move(factory)), m_worker(new QObject) {
    m_worker->moveToThread(&m_thread);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(discovery.selectedDevice(), &SelectedDevice::changed, this,
            &SettingsController::selectionChanged);
    m_thread.start();
    selectionChanged();
}

SettingsController::~SettingsController() {
    m_cancel.request_stop();
    m_thread.quit();
    m_thread.wait(); // Receive is cancellable; an in-flight output ioctl uses the kernel USB timeout.
}

void SettingsController::selectionChanged() {
    std::optional<DeviceSnapshot> next;
    const auto *selected = m_discovery.selectedDevice();
    if (selected->connected()) {
        for (const auto &device : m_discovery.snapshot().devices)
            if (device.connectionId == selected->connectionId() && Settings::supportsSettings(device))
                next = device;
    }
    if (next == m_device && !m_status.isEmpty())
        return;
    m_cancel.request_stop();
    ++m_epoch;
    m_edits.clear();
    m_editErrors.clear();
    ++m_editRevision;
    m_valuesCurrent = false;
    m_device = std::move(next);
    m_accessDenied = false;
    m_updateAccessDenied = false;
    if (m_device) {
        for (const auto &interface : m_device->hidInterfaces)
            if (interface.interfaceNumber == QStringLiteral("03"))
                m_accessDenied = !interface.canRead || !interface.canWrite;
            else if (interface.interfaceNumber == QStringLiteral("04"))
                m_updateAccessDenied = !interface.canRead || !interface.canWrite;
    }
    m_values.reset();
    m_firmware.clear();
    m_firmwareMessage = tr("Firmware version has not been read.");
    m_identityPending = m_identityEnabled && m_device.has_value();
    m_identityAttempts = 0;
    m_status = m_device ? QStringLiteral("idle") : QStringLiteral("unavailable");
    m_message = m_device ? tr("Settings will load when you open Device settings.")
                         : tr("Select a connected IMPACT 860T ANC with a supported settings interface.");
    m_readPending = m_active && m_device.has_value();
    emit changed();
    readIfPending();
}

void SettingsController::setActive(bool active) {
    if (m_active == active)
        return;
    m_active = active;
    // A current read/write already supplies fresh readback. A cancelled generation
    // must finish before the newly selected device can be read on the same worker.
    m_readPending =
        active && m_device &&
        (!m_busy || (m_status != QStringLiteral("reading") && m_status != QStringLiteral("applying")));
    emit activeChanged();
    readIfPending();
}

void SettingsController::readIfPending() {
    if (m_updateReserved)
        return;
    if (!m_busy && dispatchEdit())
        return;
    if (m_readPending && m_active && m_device && !m_busy) {
        m_readPending = false;
        start(std::nullopt);
    } else if (m_identityPending && m_identityEnabled && m_device && !m_busy && !m_accessDenied) {
        m_identityPending = false;
        startIdentityRead();
    }
}

void SettingsController::setIdentityReadsEnabled(bool enabled) {
    m_identityEnabled = enabled;
    m_identityPending = enabled && m_device && m_firmware.isEmpty();
    readIfPending();
}
void SettingsController::refreshFirmware() {
    if (!m_identityEnabled || !m_device)
        return;
    m_identityPending = true;
    m_identityAttempts = 0;
    readIfPending();
}
void SettingsController::startIdentityRead() {
    const auto device = *m_device;
    const auto epoch = ++m_epoch;
    m_cancel = std::stop_source{};
    const auto stop = m_cancel.get_token();
    ++m_identityAttempts;
    m_busy = true;
    m_firmwareMessage = tr("Reading firmware version…");
    emit changed();
    QMetaObject::invokeMethod(
        m_worker,
        [this, device, epoch, stop] {
            auto result = [&]() -> Settings::Result<Firmware::Version> {
                auto transport = m_factory(device, stop);
                if (!transport)
                    return std::unexpected(transport.error());
                Settings::Session session(**transport, stop);
                return session.readFirmware();
            }();
            QMetaObject::invokeMethod(
                this,
                [this, epoch, result = std::move(result)] {
                    m_busy = false;
                    if (epoch == m_epoch) {
                        m_firmware = result ? QString::fromStdString(result->text()) : QString();
                        m_firmwareMessage = result ? tr("Firmware version read from the headset.")
                                                   : QString::fromStdString(result.error().message);
                        if (!result && result.error().code == Settings::ErrorCode::AccessDenied)
                            m_accessDenied = true;
                        if (!result && result.error().code == Settings::ErrorCode::Busy &&
                            m_identityAttempts < 3) {
                            QTimer::singleShot(500, this, [this, epoch] {
                                if (epoch == m_epoch && m_identityEnabled) {
                                    m_identityPending = true;
                                    readIfPending();
                                }
                            });
                        }
                    }
                    emit changed();
                    readIfPending();
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

QVariantMap SettingsController::controls() const {
    QVariantMap controls;
    const auto desired = m_values ? std::optional(m_edits.projected(*m_values)) : std::nullopt;
    for (const auto feature : Settings::Features) {
        const auto blocked = editBlockReason(feature);
        auto control = SettingsPresentation::describe(feature, desired ? std::optional(desired->get(feature))
                                                                       : std::nullopt);
        control.insert("writable", blocked.isEmpty());
        control.insert("reason", blocked);
        control.insert("error", m_editErrors.value(feature));
        control.insert("revision", QVariant::fromValue(m_editRevision));
        controls.insert(QString::fromLatin1(Settings::key(feature)), control);
    }
    return controls;
}

void SettingsController::refresh() {
    if (!busy() && m_device) {
        ++m_editRevision;
        m_readPending = false;
        start(std::nullopt);
    }
}

void SettingsController::applySidetonePercent(int percent) {
    const auto raw = Settings::sidetoneFromPercent(percent);
    if (!raw || !m_values || !Settings::sidetonePercent(m_values->sidetone) ||
        Settings::sidetonePercent(m_edits.projected(*m_values).sidetone) == percent)
        return; // Do not normalize a device-native value merely by viewing its rounded percentage.
    apply(QStringLiteral("sidetone"), *raw);
}

void SettingsController::apply(const QString &key, int value) {
    if (!ready() || !m_device || m_updateReserved)
        return;
    const auto feature = Settings::featureForKey(key.toStdString());
    if (!feature || value < 0 || value > 65535)
        return;
    const auto desired = m_edits.projected(*m_values);
    Settings::Change change{*feature, static_cast<std::uint16_t>(value), desired};
    if (const auto valid = Settings::validate(change, desired); !valid) {
        m_message = QString::fromStdString(valid.error().message);
        m_editErrors[*feature] = m_message;
        emit changed();
        return;
    }
    if (!editBlockReason(*feature).isEmpty() || desired.get(*feature) == value)
        return;
    m_editErrors.remove(*feature);
    m_edits.submit({*feature, static_cast<std::uint16_t>(value)});
    readIfPending();
    emit changed();
}

void SettingsController::factoryReset(const QString &confirmedConnectionId) {
    if (busy() || !m_device || !ready() || confirmedConnectionId != m_device->connectionId)
        return;
    m_firmware.clear();
    m_identityPending = m_identityEnabled;
    ++m_editRevision;
    start(std::nullopt, true);
}

void SettingsController::start(std::optional<Settings::Change> change, bool reset) {
    const auto device = *m_device;
    const auto resetExpected = reset ? m_values : std::nullopt;
    const auto epoch = ++m_epoch;
    m_cancel = std::stop_source{};
    const auto stop = m_cancel.get_token();
    m_busy = true;
    m_status = reset || change ? QStringLiteral("applying") : QStringLiteral("reading");
    m_message = reset    ? tr("Resetting headset and reading its resulting settings…")
                : change ? tr("Changes are saved automatically.")
                         : tr("Reading headset settings…");
    if (!change)
        m_valuesCurrent = false; // Keep the last readback visible while refreshing/resetting.
    emit changed();
    QMetaObject::invokeMethod(
        m_worker,
        [this, device, epoch, stop, change, resetExpected] {
            Settings::Result<Settings::Values> result = [&]() -> Settings::Result<Settings::Values> {
                auto transport = m_factory(device, stop);
                if (!transport)
                    return std::unexpected(transport.error());
                Settings::Session session(**transport, stop);
                if (resetExpected)
                    return session.factoryReset(*resetExpected);
                return change ? session.apply(*change) : session.readAll();
            }();
            QMetaObject::invokeMethod(
                this,
                [this, epoch, result = std::move(result), change, resetExpected] {
                    m_busy = false;
                    if (epoch != m_epoch) {
                        emit changed();
                        readIfPending();
                        return; // Never publish a reply from an old selection/connection generation.
                    }
                    if (result) {
                        m_accessDenied = false;
                        m_values = *result;
                        m_valuesCurrent = true;
                        if (change) {
                            m_edits.complete();
                            m_editErrors.remove(change->feature);
                        } else {
                            m_editErrors.clear();
                        }
                        m_status = QStringLiteral("ready");
                        m_message = resetExpected
                                        ? tr("Reset acknowledged. Current settings read from the headset.")
                                        : tr("Changes are saved automatically.");
                    } else {
                        m_valuesCurrent = false;
                        for (const auto feature : Settings::Features)
                            if (m_edits.contains(feature))
                                m_editErrors[feature] = tr("Not saved. Refresh settings and try again.");
                        if (change)
                            m_editErrors[change->feature] = QString::fromStdString(result.error().message);
                        m_edits.clear();
                        m_status = QStringLiteral("error");
                        m_message = QString::fromStdString(result.error().message);
                        m_accessDenied = result.error().code == Settings::ErrorCode::AccessDenied;
                        if (m_accessDenied)
                            m_message =
                                tr("Headset access is required. Automatic setup will request administrator "
                                   "authorization; use Retry access setup if it does not complete.");
                        if (result.error().writeMayHaveApplied)
                            m_message +=
                                resetExpected
                                    ? tr(" Reset may have applied. Waiting for the headset to return; "
                                         "its settings will reload when it reconnects. "
                                         "It will not be retried automatically.")
                                    : tr(" The setting may have changed. Read settings again before "
                                         "making another change.");
                    }
                    emit changed();
                    readIfPending();
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}
std::optional<DeviceSnapshot> SettingsController::reserveForUpdate(const QString &connectionId) {
    if (busy() || !m_device || m_device->connectionId != connectionId)
        return {};
    const auto device = m_device;
    m_updateReserved = true;
    emit changed();
    return device;
}
void SettingsController::releaseFromUpdate() {
    if (!m_updateReserved)
        return;
    m_updateReserved = false;
    m_values.reset();
    m_valuesCurrent = false;
    ++m_editRevision;
    m_firmware.clear();
    m_readPending = m_active && m_device.has_value();
    m_identityPending = m_identityEnabled && m_device.has_value();
    m_identityAttempts = 0;
    emit changed();
    readIfPending();
}
QString SettingsController::editBlockReason(Settings::Feature feature) const {
    if (!m_device)
        return tr("Connect a headset to change this setting.");
    if (m_updateReserved)
        return tr("Settings are unavailable during firmware installation.");
    if (!ready() || m_accessDenied)
        return tr("Read the headset settings before making changes.");
    const auto desired = m_edits.projected(*m_values);
    if (const auto valid = Settings::prerequisites(feature, desired); !valid)
        return QString::fromStdString(valid.error().message);
    if (feature == Settings::Feature::Sidetone && !Settings::sidetonePercent(m_values->sidetone))
        return tr("This headset reports an unsupported sidetone value.");
    return {};
}
bool SettingsController::dispatchEdit() {
    if (!m_device || !ready())
        return false;
    while (const auto edit = m_edits.take()) {
        Settings::Change change{edit->feature, edit->value, *m_values};
        if (const auto valid = Settings::validate(change, *m_values); !valid) {
            m_editErrors[edit->feature] = QString::fromStdString(valid.error().message);
            m_edits.complete();
            emit changed();
            continue;
        }
        if (m_values->get(edit->feature) == edit->value) {
            m_edits.complete();
            emit changed();
            continue;
        }
        start(change); // Rebase only on confirmed results; Session still performs a fresh preflight.
        return true;
    }
    return false;
}
