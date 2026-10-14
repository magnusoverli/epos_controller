#pragma once
#include <expected>
#include <string>
#include <string_view>

namespace DeviceAccess {
inline constexpr auto RuleName = "70-headset-controller.rules";
// The caller opens the destination directory. Only this fixed filename is managed.
std::expected<void, std::string> installRule(int directory, std::string_view contents);
} // namespace DeviceAccess
