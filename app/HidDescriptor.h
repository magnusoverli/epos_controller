#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace HidDescriptor {

// Resource budgets for untrusted descriptor bytes, not protocol authorization.
inline constexpr std::size_t MaxDescriptorBytes = 8192;
inline constexpr std::size_t MaxNesting = 64;
inline constexpr std::uint64_t MaxReportBits = 65536 * 8;

enum class ReportKind : std::uint8_t { Feature, Input, Output };

struct Report {
    ReportKind kind;
    std::uint8_t id;
    std::uint32_t payloadBytes;

    [[nodiscard]] constexpr std::uint32_t wireBytes() const noexcept {
        return payloadBytes + (id != 0 ? 1U : 0U);
    }
    bool operator==(const Report &) const = default;
};

struct Description {
    std::vector<std::uint32_t> usagePages;
    std::vector<Report> reports;
};

enum class ErrorCode : std::uint8_t {
    Empty, TooLarge, LongItem, TruncatedItem, InvalidItemSize, InvalidReportId,
    GlobalUnderflow, CollectionUnderflow, Unbalanced, NestingLimit,
    ReportTooLarge, EmptyReport, MixedReportIds
};

struct ParseError {
    ErrorCode code;
    std::size_t offset;
    bool operator==(const ParseError &) const = default;
};

using Result = std::expected<Description, ParseError>;

// The input is borrowed for this call only; results never retain references.
// This inspects report layout, not every semantic constraint of the HID spec.
[[nodiscard]] Result parse(std::span<const std::byte> bytes);
[[nodiscard]] std::string_view message(ErrorCode error) noexcept;
[[nodiscard]] std::string_view name(ReportKind kind) noexcept;

}
