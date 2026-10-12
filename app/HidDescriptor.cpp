#include "HidDescriptor.h"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <utility>

namespace HidDescriptor {
namespace {
struct Globals {
    std::uint32_t size = 0;
    std::uint32_t count = 0;
    std::uint8_t id = 0;
};

[[nodiscard]] constexpr std::optional<ReportKind> reportKind(unsigned tag) noexcept {
    switch (tag) {
    case 8: return ReportKind::Input;
    case 9: return ReportKind::Output;
    case 11: return ReportKind::Feature;
    default: return std::nullopt;
    }
}
}

Result parse(std::span<const std::byte> bytes) {
    if (bytes.empty()) return std::unexpected(ParseError{ErrorCode::Empty, 0});
    if (bytes.size() > MaxDescriptorBytes) {
        return std::unexpected(ParseError{ErrorCode::TooLarge, MaxDescriptorBytes});
    }

    Globals state;
    std::array<Globals, MaxNesting> stack{};
    std::size_t stackSize = 0;
    std::size_t collections = 0;
    std::map<std::pair<ReportKind, std::uint8_t>, std::uint64_t> reportBits;
    Description result;
    bool numbered = false;
    bool unnumbered = false;

    for (std::size_t pos = 0; pos < bytes.size();) {
        const auto offset = pos;
        const auto fail = [offset](ErrorCode code) { return std::unexpected(ParseError{code, offset}); };
        const auto prefix = std::to_integer<unsigned>(bytes[pos++]);
        if (prefix == 0xfe) return fail(ErrorCode::LongItem);

        const auto size = (prefix & 3U) == 3U ? 4U : prefix & 3U;
        if (size > bytes.size() - pos) return fail(ErrorCode::TruncatedItem);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < size; ++i) {
            value |= std::to_integer<std::uint32_t>(bytes[pos++]) << (8U * i);
        }
        const auto type = (prefix >> 2U) & 3U;
        const auto tag = prefix >> 4U;
        if (type == 1) {
            switch (tag) {
            case 0:
                if (size == 0) return fail(ErrorCode::InvalidItemSize);
                result.usagePages.push_back(value);
                break;
            case 7:
                if (size == 0) return fail(ErrorCode::InvalidItemSize);
                state.size = value;
                break;
            case 8:
                if (size != 1) return fail(ErrorCode::InvalidItemSize);
                if (value == 0 || value > 255) return fail(ErrorCode::InvalidReportId);
                state.id = static_cast<std::uint8_t>(value);
                numbered = true;
                if (unnumbered) return fail(ErrorCode::MixedReportIds);
                break;
            case 9:
                if (size == 0) return fail(ErrorCode::InvalidItemSize);
                state.count = value;
                break;
            case 10:
                if (size != 0) return fail(ErrorCode::InvalidItemSize);
                if (stackSize == stack.size()) return fail(ErrorCode::NestingLimit);
                stack[stackSize++] = state;
                break;
            case 11:
                if (size != 0) return fail(ErrorCode::InvalidItemSize);
                if (stackSize == 0) return fail(ErrorCode::GlobalUnderflow);
                state = stack[--stackSize];
                break;
            default:
                break;
            }
        } else if (type == 0) {
            if (tag == 10) {
                if (size != 1) return fail(ErrorCode::InvalidItemSize);
                if (collections == MaxNesting) return fail(ErrorCode::NestingLimit);
                ++collections;
            } else if (tag == 12) {
                if (size != 0) return fail(ErrorCode::InvalidItemSize);
                if (collections == 0) return fail(ErrorCode::CollectionUnderflow);
                --collections;
            } else if (const auto kind = reportKind(tag)) {
                if (size == 0) return fail(ErrorCode::InvalidItemSize);
                if (state.size == 0 || state.count == 0) return fail(ErrorCode::EmptyReport);
                if (state.id == 0) unnumbered = true;
                if (numbered && unnumbered) return fail(ErrorCode::MixedReportIds);
                const auto extra = std::uint64_t{state.size} * state.count;
                auto &bits = reportBits[{*kind, state.id}];
                // Check before addition; the widened product cannot overflow.
                if (extra > MaxReportBits || bits > MaxReportBits - extra) {
                    return fail(ErrorCode::ReportTooLarge);
                }
                bits += extra;
            }
        }
    }
    if (collections != 0 || stackSize != 0) {
        return std::unexpected(ParseError{ErrorCode::Unbalanced, bytes.size()});
    }

    std::ranges::sort(result.usagePages);
    const auto duplicates = std::ranges::unique(result.usagePages);
    result.usagePages.erase(duplicates.begin(), duplicates.end());
    result.reports.reserve(reportBits.size());
    for (const auto &[key, bits] : reportBits) {
        const auto &[kind, id] = key;
        result.reports.push_back({kind, id, static_cast<std::uint32_t>((bits + 7) / 8)});
    }
    return result;
}

std::string_view name(ReportKind kind) noexcept {
    switch (kind) {
    case ReportKind::Feature: return "feature";
    case ReportKind::Input: return "input";
    case ReportKind::Output: return "output";
    }
    return "unknown";
}

std::string_view message(ErrorCode error) noexcept {
    switch (error) {
    case ErrorCode::Empty: return "Descriptor empty or unreadable";
    case ErrorCode::TooLarge: return "Descriptor exceeds inspection size limit";
    case ErrorCode::LongItem: return "Long HID items are not supported by this inspector";
    case ErrorCode::TruncatedItem: return "Truncated HID item";
    case ErrorCode::InvalidItemSize: return "Invalid HID item size";
    case ErrorCode::InvalidReportId: return "Invalid report ID";
    case ErrorCode::GlobalUnderflow: return "Unbalanced global pop";
    case ErrorCode::CollectionUnderflow: return "Unbalanced collection";
    case ErrorCode::Unbalanced: return "Unbalanced descriptor";
    case ErrorCode::NestingLimit: return "Descriptor exceeds nesting limit";
    case ErrorCode::ReportTooLarge: return "Implausible report length";
    case ErrorCode::EmptyReport: return "Report size or count is zero";
    case ErrorCode::MixedReportIds: return "Mixed numbered and unnumbered reports";
    }
    return "Unknown descriptor error";
}

}
