#pragma once
#include <charconv>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Firmware {
struct Version {
    std::uint16_t major;
    std::uint16_t minor;
    std::uint16_t patch;
    auto operator<=>(const Version &) const = default;
    std::string text() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};
inline std::optional<Version> parseVersion(std::string_view text) {
    std::uint16_t parts[3]{};
    for (int i = 0; i < 3; ++i) {
        const auto dot = text.find('.');
        if ((i < 2) != (dot != std::string_view::npos))
            return std::nullopt;
        const auto part = text.substr(0, dot);
        if (part.empty() || part.size() > 5 || part.front() < '0' || part.front() > '9')
            return std::nullopt;
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), parts[i]);
        if (parsed.ec != std::errc{} || parsed.ptr != part.data() + part.size())
            return std::nullopt;
        if (i < 2)
            text.remove_prefix(dot + 1);
    }
    return Version{parts[0], parts[1], parts[2]};
}
} // namespace Firmware
