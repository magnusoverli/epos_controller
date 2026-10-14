#pragma once
#include "DeviceSnapshot.h"
#include <QProcess>
#include <QTimer>
#include <functional>

namespace Notifications {
// PulseAudio's client tools also work with PipeWire's PulseAudio server.
// Playback is explicitly routed to this headset, never to an arbitrary default sink.
class AlertSound final : public QObject {
  public:
    using Completion = std::function<void(QString)>;
    explicit AlertSound(QObject *parent = nullptr);
    ~AlertSound() override;
    void play(const DeviceSnapshot &device, Completion done);
    void stop();
    static QString headsetSink(const QByteArray &json, const QString &usbKey);
    static QByteArray tone();

  private:
    void finish(QString error);
    QProcess m_process;
    QTimer m_deadline;
    Completion m_done;
    QByteArray m_output;
    QString m_key;
    QString m_player;
    bool m_listing = false;
};
} // namespace Notifications
