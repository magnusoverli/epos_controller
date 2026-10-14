#pragma once
#include <string_view>
namespace CapturedFirmware {
// Windows enumeration frames 135/137. Independent captured getter, version 1.5.64.
inline constexpr std::string_view Request = "01ebfe0100000001000400f80001000300";
inline constexpr std::string_view Reply = "01ebfe0100000000000a004c0001000300010005004000";
} // namespace CapturedFirmware
