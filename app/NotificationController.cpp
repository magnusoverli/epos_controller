#include "NotificationController.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <array>
#include <cerrno>
#include <cstring>
#include <unistd.h>

NotificationController::NotificationController(DeviceDiscovery &discovery, bool runtimeAllowed,
                                               QString preferencesPath, InputFactory factory, Sender sender,
                                               QObject *parent, AudioSender audioSender)
    : QObject(parent), m_discovery(discovery), m_allowed(runtimeAllowed),
      m_path(preferencesPath.isEmpty()
                 ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                       QStringLiteral("/headset-controller/notifications.ini")
                 : std::move(preferencesPath)),
      m_factory(std::move(factory)), m_sender(std::move(sender)), m_audioSender(std::move(audioSender)) {
    if (!m_audioSender)
        m_audioSender = [this](const DeviceSnapshot &device, Completion done) {
            m_sound.play(device, std::move(done));
        };
    if (!m_sender)
        m_sender = [this](bool audio, Completion done) { deliver(audio, std::move(done)); };
    connect(discovery.selectedDevice(), &SelectedDevice::changed, this,
            &NotificationController::selectionChanged);
    selectionChanged();
}
NotificationController::~NotificationController() {
    stop();
}

QString NotificationController::preferenceKey() const {
    // No serial is collected: preferences are explicitly scoped to model + USB port.
    return QString::fromLatin1((m_device->usbId + '/' + m_device->key).toUtf8().toHex());
}
void NotificationController::selectionChanged() {
    std::optional<DeviceSnapshot> next;
    const auto *selected = m_discovery.selectedDevice();
    if (selected->connected())
        for (const auto &device : m_discovery.snapshot().devices)
            if (device.connectionId == selected->connectionId() && Settings::supportsSettings(device))
                next = device;
    if (next == m_device && !m_message.isEmpty())
        return;
    m_device = std::move(next);
    m_preferences = {};
    if (m_device) {
        QSettings settings(m_path, QSettings::IniFormat);
        settings.beginGroup(preferenceKey());
        bool modeOk = false;
        bool intervalOk = false;
        const auto mode = settings.value("mode", 0).toInt(&modeOk);
        const auto interval = settings.value("intervalMinutes", 0).toInt(&intervalOk);
        const Notifications::Preferences stored{static_cast<Notifications::Mode>(mode), interval};
        if (modeOk && intervalOk && Notifications::valid(stored))
            m_preferences = stored;
    }
    restart();
}
void NotificationController::stop() {
    ++m_generation;
    m_sound.stop();
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    m_pending = false;
    m_policy = Notifications::Policy(m_preferences);
}
void NotificationController::restart() {
    stop();
    m_failed = false;
    if (!m_allowed)
        m_message = tr("Desktop notifications are disabled in automation mode.");
    else if (!m_device)
        m_message = tr("Select a supported headset for desktop mute notifications.");
    else if (m_preferences.mode == Notifications::Mode::Off)
        m_message =
            tr("Desktop mute notifications are off. Preferences are saved on this computer per USB port.");
    else {
        auto input = m_factory(*m_device);
        if (!input) {
            m_failed = true;
            m_message =
                tr("Cannot listen for mute events: %1").arg(QString::fromStdString(input.error().message));
        } else {
            m_fd = *input;
            m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
            const auto generation = m_generation;
            connect(m_notifier, &QSocketNotifier::activated, this, [this, generation] {
                if (generation == m_generation)
                    receive();
            });
            m_message =
                tr("Listening for speaking-while-muted events. Interval limits notification frequency. Keep "
                   "this app running. Audio alerts play through the selected headset.");
        }
    }
    emit changed();
}
void NotificationController::retry() {
    if (canRetry())
        restart();
}
QVariantMap NotificationController::controls() const {
    auto choice = [](const QString &label, int value) {
        return QVariantMap{{"label", label}, {"value", value}};
    };
    auto control = [&](QVariantList options, int value, bool writable) {
        int index = -1;
        for (qsizetype i = 0; i < options.size(); ++i)
            if (options[i].toMap().value("value").toInt() == value)
                index = static_cast<int>(i);
        return QVariantMap{{"options", options},
                           {"currentIndex", index},
                           {"display", index >= 0 ? options[index].toMap().value("label") : QVariant()},
                           {"writable", writable}};
    };
    const bool writable = m_allowed && m_device.has_value();
    return {{"muteNotification",
             control({choice(tr("Off"), 0), choice(tr("Visual only"), 1), choice(tr("Visual and audio"), 2)},
                     static_cast<int>(m_preferences.mode), writable)},
            {"muteInterval", control({choice(tr("When I start speaking"), 0), choice(tr("1 minute"), 1),
                                      choice(tr("5 minutes"), 5), choice(tr("10 minutes"), 10)},
                                     m_preferences.intervalMinutes,
                                     writable && m_preferences.mode != Notifications::Mode::Off)}};
}
void NotificationController::apply(const QString &key, int value) {
    if (!m_allowed || !m_device)
        return;
    auto next = m_preferences;
    if (key == QStringLiteral("muteNotification"))
        next.mode = static_cast<Notifications::Mode>(value);
    else if (key == QStringLiteral("muteInterval") && next.mode != Notifications::Mode::Off)
        next.intervalMinutes = value;
    else
        return;
    if (!Notifications::valid(next) || next == m_preferences)
        return;
    QSettings settings(m_path, QSettings::IniFormat);
    settings.beginGroup(preferenceKey());
    settings.setValue("mode", static_cast<int>(next.mode));
    settings.setValue("intervalMinutes", next.intervalMinutes);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        m_message = tr("Could not save desktop notification preferences.");
        emit changed();
        return;
    }
    m_preferences = next;
    restart();
}
void NotificationController::receive() {
    // Bounded work per activation keeps a busy HID stream from starving the UI.
    for (int count = 0; count < 32 && m_fd >= 0; ++count) {
        std::array<std::uint8_t, 4096> bytes{};
        const auto size = ::read(m_fd, bytes.data(), bytes.size());
        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return;
        if (size < 0 && errno == EINTR)
            continue;
        if (size <= 0) {
            const auto detail =
                size == 0 ? tr("Headset disconnected") : QString::fromLocal8Bit(std::strerror(errno));
            stop();
            m_failed = true;
            m_message = tr("Mute event listener stopped: %1").arg(detail);
            emit changed();
            return;
        }
        const auto event = Notifications::decode(std::span(bytes).first(static_cast<std::size_t>(size)));
        if (!event || !m_policy.observe(*event, Notifications::Policy::Clock::now()) || m_pending)
            continue;
        sendNotification();
    }
}
void NotificationController::testNotification() {
    if (canTest())
        sendNotification();
}
void NotificationController::sendNotification() {
    m_pending = true;
    emit changed();
    const auto generation = m_generation;
    const QPointer<NotificationController> self(this);
    m_sender(m_preferences.mode == Notifications::Mode::VisualAndAudio, [self, generation](QString error) {
        if (!self || generation != self->m_generation)
            return;
        self->m_pending = false;
        if (!error.isEmpty()) {
            self->m_failed = true;
            self->m_message = self->tr("Desktop notification failed: %1").arg(error);
        } else if (self->m_failed) {
            self->m_failed = false;
            self->m_message = self->tr("Desktop mute notifications are active. Interval limits "
                                       "speech-triggered notification frequency.");
        }
        emit self->changed();
    });
}
void NotificationController::deliver(bool audio, Completion completion) {
    struct Delivery {
        int pending;
        QStringList errors;
        Completion complete;
    };
    auto delivery = std::make_shared<Delivery>(Delivery{audio ? 2 : 1, {}, std::move(completion)});
    auto completed = [delivery](QString error) {
        if (!error.isEmpty())
            delivery->errors.append(error);
        if (--delivery->pending == 0)
            delivery->complete(delivery->errors.join(' '));
    };
    if (audio && m_device)
        m_audioSender(*m_device, completed);
    else if (audio)
        completed(tr("The headset disconnected before audio playback."));
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Notify"));
    QVariantMap hints{{"suppress-sound", true},
                      {"transient", true},
                      {"desktop-entry", QStringLiteral("headset-controller")}};
    message << QStringLiteral("Headset Controller") << uint(0) << QStringLiteral("audio-input-microphone")
            << tr("Microphone muted") << tr("You are speaking while your headset microphone is muted.")
            << QStringList{} << hints << 5000;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 2000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, done = std::move(completed)] {
        const QDBusPendingReply<uint> reply = *watcher;
        done(reply.isError() ? reply.error().message() : QString());
        watcher->deleteLater();
    });
}
