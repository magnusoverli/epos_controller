#pragma once
#include "DeviceDiscovery.h"
#include "notifications/AlertSound.h"
#include "notifications/MuteEvents.h"
#include "settings/LinuxHidTransport.h"
#include <QSocketNotifier>
#include <QVariantMap>
#include <functional>

// Host preferences and passive event presentation are independent of scalar
// device settings and their read/write transaction worker.
class NotificationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap controls READ controls NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)
    Q_PROPERTY(bool canTest READ canTest NOTIFY changed)
  public:
    using InputFactory = std::function<Settings::Result<int>(const DeviceSnapshot &)>;
    using Completion = std::function<void(QString)>;
    using Sender = std::function<void(bool, Completion)>;
    using AudioSender = std::function<void(const DeviceSnapshot &, Completion)>;
    explicit NotificationController(DeviceDiscovery &discovery, bool runtimeAllowed = true,
                                    QString preferencesPath = {},
                                    InputFactory factory = Settings::openHidEventInput, Sender sender = {},
                                    QObject *parent = nullptr, AudioSender audioSender = {});
    ~NotificationController() override;
    QVariantMap controls() const;
    QString message() const { return m_message; }
    bool canRetry() const { return m_allowed && m_device && m_failed; }
    bool canTest() const {
        return m_allowed && m_device && !m_pending && m_preferences.mode != Notifications::Mode::Off;
    }
    Q_INVOKABLE void apply(const QString &key, int value);
    Q_INVOKABLE void retry();
    Q_INVOKABLE void testNotification();
  signals:
    void changed();

  private:
    void selectionChanged();
    void restart();
    void stop();
    void receive();
    void sendNotification();
    void deliver(bool audio, Completion completion);
    QString preferenceKey() const;
    DeviceDiscovery &m_discovery;
    bool m_allowed;
    QString m_path;
    InputFactory m_factory;
    Sender m_sender;
    Notifications::AlertSound m_sound;
    AudioSender m_audioSender;
    std::optional<DeviceSnapshot> m_device;
    Notifications::Preferences m_preferences;
    Notifications::Policy m_policy;
    QString m_message;
    int m_fd = -1;
    QSocketNotifier *m_notifier = nullptr;
    quint64 m_generation = 0;
    bool m_pending = false;
    bool m_failed = false;
};
