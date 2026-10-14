#pragma once
#include "SettingsCaptureFixtures.h"

// Independent reads from 00-enumeration-reviewed.pcapng; see fixtures/settings/EXTENDED.md.
namespace EnumeratedSettings {
inline constexpr CapturedSettings::Pair Pairs[]{
    // Enumeration frames 217/219.
    {Settings::Feature::AudioLimiter, false, 0, "01ebfe01000000010004005b000a001500",
     "01ebfe010000000000080076000a00150000000000"},
    // Enumeration frames 225/227.
    {Settings::Feature::AnswerOnWear, false, 0, "01ebfe0100000001000a00a9000800ef000a0001000200",
     "01ebfe0100000000000800ee000800ef000a000000"},
    // Enumeration frames 233/235.
    {Settings::Feature::MuteOnRemoval, false, 0, "01ebfe0100000001000a00bf000800ef000a0000000200",
     "01ebfe0100000000000800ee000800ef000a000000"},
    // Enumeration frames 309/311.
    {Settings::Feature::SmartPause, false, 1, "01ebfe0100000001000a0093000800ef000a0002000200",
     "01ebfe0100000000000800fb000800ef000a000100"},
    // Enumeration frames 241/243.
    {Settings::Feature::BoomSide, false, 0, "01ebfe0100000001000400ce0010002e00",
     "01ebfe01000000000006003b0010002e000000"},
    // Enumeration frames 269/271.
    {Settings::Feature::NoPromptsDuringCalls, false, 1, "01ebfe0100000001000400310007000a00",
     "01ebfe0100000000000600f90007000a000100"},
    // Enumeration frames 277/279.
    {Settings::Feature::ProductRingtone, false, 0, "01ebfe0100000001000400090009000600",
     "01ebfe0100000000000600bd00090006000000"},
    // Enumeration frames 285/287.
    {Settings::Feature::Busylight, false, 0, "01ebfe0100000001000400850014000700",
     "01ebfe0100000000000600f700140007000000"},
    // Enumeration frames 293/295.
    {Settings::Feature::TalkWhileMuted, false, 1, "01ebfe01000000010004004f0010003000",
     "01ebfe01000000000006008d00100030000100"},
    // Enumeration frames 301/303.
    {Settings::Feature::ButtonStatus, false, 0, "01ebfe0100000001000400670008002800",
     "01ebfe01000000000006009e00080028000000"},
    // Enumeration frames 329/331.
    {Settings::Feature::AnswerOnBoomDown, false, 1, "01ebfe0100000001000600430008001e000000",
     "01ebfe0100000000000600560008001e000100"},
    // Enumeration frames 318/322.
    {Settings::Feature::MuteOnBoomUp, false, 1, "01ebfe0100000001000600560008001e000100",
     "01ebfe0100000000000600560008001e000100"},
    // Enumeration frames 337/339.
    {Settings::Feature::AutoPause, false, 0, "01ebfe01000000010004003e0001003000",
     "01ebfe01000000000006002f00010030000000"},
    // Enumeration frames 249/251.
    {Settings::Feature::Prompts, false, 1, "01ebfe0100000001000400f20007000500",
     "01ebfe01000000000006002b00070005000100"},
    // Enumeration frames 157/159. English voice prompts.
    {Settings::Feature::PromptLanguage, false, 0, "01ebfe01000000010004008c0007000300",
     "01ebfe01000000000006004a00070003000000"},
};
} // namespace EnumeratedSettings
