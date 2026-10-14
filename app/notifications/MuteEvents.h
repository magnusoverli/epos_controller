#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>

namespace Notifications {
enum class EventKind { SpeakingWhileMuted, MicrophoneMuted };
struct Event {
    EventKind kind;
    bool active;
    bool operator==(const Event &) const = default;
};
// Strict, read-only decoder for the observed IMPACT 860T ANC event envelope.
std::optional<Event> decode(std::span<const std::uint8_t> report);

enum class Mode { Off, Visual, VisualAndAudio };
struct Preferences {
    Mode mode = Mode::Off;
    int intervalMinutes = 0;
    bool operator==(const Preferences &) const = default;
};
bool valid(const Preferences &preferences);

class Policy {
  public:
    using Clock = std::chrono::steady_clock;
    explicit Policy(Preferences preferences = {}) : m_preferences(preferences) {}
    // Intervals throttle speech-triggered notifications; expiry alone never notifies.
    bool observe(Event event, Clock::time_point now);

  private:
    Preferences m_preferences;
    bool m_speaking = false;
    std::optional<Clock::time_point> m_lastNotification;
};
} // namespace Notifications
