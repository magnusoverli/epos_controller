#include "MuteEvents.h"
#include "settings/EposProtocol.h"
#include <algorithm>

namespace Notifications {
std::optional<Event> decode(std::span<const std::uint8_t> report) {
    constexpr std::uint8_t header[]{1, 0xeb, 0xfe, 1, 0, 1, 1, 0, 0, 8, 0};
    if (report.size() != EposProtocol::ReportSize ||
        !std::equal(std::begin(header), std::end(header), report.begin()) || report[12] != 0 ||
        EposProtocol::checksum(report.subspan(13, 8)) != report[11] ||
        !std::all_of(report.begin() + 21, report.end(), [](auto byte) { return byte == 0; }))
        return std::nullopt;
    if (report[13] != 0x10 || report[14] != 0 || report[15] != 0xf0 || report[16] != 0 || report[18] != 0 ||
        report[19] > 1 || report[20] != 0)
        return std::nullopt;
    if (report[17] == 1)
        return Event{EventKind::SpeakingWhileMuted, report[19] == 1};
    if (report[17] == 2)
        return Event{EventKind::MicrophoneMuted, report[19] == 1};
    return std::nullopt;
}
bool valid(const Preferences &preferences) {
    return (preferences.mode == Mode::Off || preferences.mode == Mode::Visual ||
            preferences.mode == Mode::VisualAndAudio) &&
           (preferences.intervalMinutes == 0 || preferences.intervalMinutes == 1 ||
            preferences.intervalMinutes == 5 || preferences.intervalMinutes == 10);
}
bool Policy::observe(Event event, Clock::time_point now) {
    if (event.kind == EventKind::MicrophoneMuted) {
        if (!event.active)
            m_speaking = false;
        return false;
    }
    const bool rising = event.active && !m_speaking;
    m_speaking = event.active;
    if (!rising || !valid(m_preferences) || m_preferences.mode == Mode::Off)
        return false;
    if (m_lastNotification && now - *m_lastNotification < std::chrono::minutes(m_preferences.intervalMinutes))
        return false;
    m_lastNotification = now;
    return true;
}
} // namespace Notifications
