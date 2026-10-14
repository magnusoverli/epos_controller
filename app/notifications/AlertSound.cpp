#include "AlertSound.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace Notifications {
QString AlertSound::headsetSink(const QByteArray &json, const QString &usbKey) {
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isArray() || usbKey.isEmpty())
        return {};
    QString result;
    for (const auto &entry : doc.array()) {
        const auto sink = entry.toObject();
        const auto props = sink["properties"].toObject();
        auto id = [&](const char *key) {
            bool ok = false;
            const auto value = props[key].toString().toUInt(&ok, 16);
            return ok ? value : 0;
        };
        if (id("device.vendor.id") != 0x1395 || id("device.product.id") != 0x018e ||
            !props["sysfs.path"].toString().split('/').contains(usbKey))
            continue;
        if (!result.isEmpty())
            return {}; // Ambiguous audio profile; never play on a guessed output.
        result = sink["name"].toString();
    }
    return result;
}
QByteArray AlertSound::tone() {
    // Original two-note cue: 16-bit LE mono, 48 kHz, 0.4 s, quiet peak with fades.
    QByteArray pcm(19200 * 2, '\0');
    for (int i = 0; i < 19200; ++i) {
        const int within = i % 9600;
        if (within >= 7200)
            continue;
        const double envelope = std::min({1.0, within / 480.0, (7200 - within) / 480.0});
        const double frequency = i < 9600 ? 660.0 : 880.0;
        const auto sample = static_cast<qint16>(4000 * envelope *
                                                std::sin(2 * std::numbers::pi * frequency * within / 48000));
        qToLittleEndian(sample, pcm.data() + 2 * i);
    }
    return pcm;
}
AlertSound::AlertSound(QObject *parent) : QObject(parent) {
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] { finish(tr("Headset alert playback timed out.")); });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
        if (m_output.size() > 1024 * 1024)
            finish(tr("Audio device list is too large."));
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_process.readAllStandardError(); // Drain bounded process pipes; do not expose device identifiers.
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            finish(tr("Could not start headset audio playback: %1").arg(m_process.errorString()));
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!m_done)
            return;
        if (code != 0 || status != QProcess::NormalExit) {
            finish(tr("Headset audio playback failed. Check the desktop audio service and headset output."));
            return;
        }
        if (!m_listing) {
            finish({});
            return;
        }
        m_output += m_process.readAllStandardOutput();
        const auto sink = headsetSink(m_output, m_key);
        if (sink.isEmpty()) {
            finish(tr("No unique audio output found for the selected headset. Enable its output in Sound "
                      "settings."));
            return;
        }
        m_listing = false;
        m_output.clear();
        m_process.start(m_player,
                        {"--device=" + sink, "--raw", "--format=s16le", "--rate=48000", "--channels=1",
                         "--client-name=Headset Controller", "--stream-name=Microphone muted alert",
                         "--property=media.role=notification"});
        m_process.write(tone());
        m_process.closeWriteChannel();
    });
}
AlertSound::~AlertSound() {
    stop();
}
void AlertSound::stop() {
    m_done = {};
    m_deadline.stop();
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
    m_output.clear();
}
void AlertSound::finish(QString error) {
    auto done = std::move(m_done);
    stop();
    if (done)
        done(std::move(error));
}
void AlertSound::play(const DeviceSnapshot &device, Completion done) {
    stop();
    m_done = std::move(done);
    m_key = device.key;
    m_player = QStandardPaths::findExecutable("paplay", {"/usr/bin", "/bin"});
    const auto pactl = QStandardPaths::findExecutable("pactl", {"/usr/bin", "/bin"});
    if (m_player.isEmpty() || pactl.isEmpty()) {
        finish(tr("Headset audio alerts require paplay and pactl (pulseaudio-utils)."));
        return;
    }
    m_listing = true;
    m_deadline.start(4000);
    m_process.start(pactl, {"--format=json", "list", "sinks"});
}
} // namespace Notifications
