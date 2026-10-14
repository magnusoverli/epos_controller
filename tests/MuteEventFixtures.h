#pragma once
#include "SettingsCaptureFixtures.h"
#include "notifications/MuteEvents.h"

// Captured on Linux, not generated from the production decoder. Full report
// padding is zero. See fixtures/settings/MUTE_EVENTS.md for timestamps and hashes.
namespace CapturedMuteEvents {
struct Fixture {
    std::string_view hex;
    Notifications::Event event;
};
inline constexpr Fixture Reports[]{
    {"01ebfe0100010100000800d0001000f00001000100", {Notifications::EventKind::SpeakingWhileMuted, true}},
    {"01ebfe0100010100000800c5001000f00001000000", {Notifications::EventKind::SpeakingWhileMuted, false}},
    {"01ebfe0100010100000800ea001000f00002000100", {Notifications::EventKind::MicrophoneMuted, true}},
    {"01ebfe0100010100000800ff001000f00002000000", {Notifications::EventKind::MicrophoneMuted, false}},
};
} // namespace CapturedMuteEvents
