#include "CapturedDescriptor.h"
#include "HidDescriptor.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string_view>

namespace {
void check(bool condition, std::string_view label,
           const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "%s:%u: %.*s\n", location.file_name(), location.line(),
                     static_cast<int>(label.size()), label.data());
        std::exit(EXIT_FAILURE);
    }
}

std::vector<std::byte> hex(std::string_view text) {
    check(text.size() % 2 == 0, "even hex length");
    std::vector<std::byte> bytes;
    for (std::size_t i = 0; i < text.size(); i += 2) {
        unsigned value = 0;
        const auto [end, error] = std::from_chars(text.data() + i, text.data() + i + 2, value, 16);
        check(error == std::errc{} && end == text.data() + i + 2, "valid fixture hex");
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

void rejects(std::span<const std::byte> bytes, HidDescriptor::ErrorCode code, std::size_t offset) {
    const auto result = HidDescriptor::parse(bytes);
    check(!result, "malformed descriptor rejected");
    check(result.error() == HidDescriptor::ParseError{code, offset}, "precise error code and byte offset");
}
} // namespace

int main() {
    using namespace HidDescriptor;
    const auto captured = hex(CapturedDescriptorHex);
    const auto result = parse(captured);
    check(result.has_value(), "real headset descriptor accepted");
    const std::vector<Report> expected{{ReportKind::Feature, 3, 3},  {ReportKind::Feature, 4, 3},
                                       {ReportKind::Input, 2, 65},   {ReportKind::Input, 6, 12},
                                       {ReportKind::Output, 1, 254}, {ReportKind::Output, 5, 254}};
    check(result->reports == expected, "all six captured report layouts match");
    check(result->usagePages == std::vector<std::uint32_t>{0xff00}, "vendor usage page");
    check(result->reports.back().wireBytes() == 255, "numbered report includes ID");

    const auto fractional = parse(hex("75019501810295078102"));
    check(fractional && fractional->reports.front().wireBytes() == 1, "round after aggregating fields");
    const auto pushed = parse(hex("850175089501a4750195018102b48102"));
    check(pushed && pushed->reports.front().wireBytes() == 3, "global push/pop restores report state");

    const auto sameId = parse(hex("85017508950181029102b102"));
    check(sameId && sameId->reports == std::vector<Report>{{ReportKind::Feature, 1, 1},
                                                           {ReportKind::Input, 1, 1},
                                                           {ReportKind::Output, 1, 1}},
          "same ID remains independent across report kinds");
    const auto pages = parse(hex("0501050c0501"));
    check(pages && pages->usagePages == std::vector<std::uint32_t>{1, 12},
          "usage pages sorted and deduplicated");
    const auto largest = parse(hex("750897000001008102"));
    check(largest && largest->reports.front().payloadBytes == MaxReportBits / 8,
          "exact report budget accepted");
    std::vector<std::byte> full;
    for (std::size_t i = 0; i < MaxDescriptorBytes / 2; ++i) {
        full.push_back(std::byte{0x05});
        full.push_back(std::byte{0x01});
    }
    check(parse(full).has_value(), "exact descriptor byte budget accepted");
    std::vector<std::byte> globals(MaxNesting, std::byte{0xa4});
    globals.insert(globals.end(), MaxNesting, std::byte{0xb4});
    check(parse(globals).has_value(), "exact global nesting budget accepted");
    std::vector<std::byte> collections;
    for (std::size_t i = 0; i < MaxNesting; ++i) {
        collections.push_back(std::byte{0xa1});
        collections.push_back(std::byte{0x01});
    }
    collections.insert(collections.end(), MaxNesting, std::byte{0xc0});
    check(parse(collections).has_value(), "exact collection nesting budget accepted");

    rejects({}, ErrorCode::Empty, 0);
    rejects(hex("26ff"), ErrorCode::TruncatedItem, 0);
    rejects(hex("fe"), ErrorCode::LongItem, 0);
    rejects(hex("b4"), ErrorCode::GlobalUnderflow, 0);
    rejects(hex("c0"), ErrorCode::CollectionUnderflow, 0);
    rejects(hex("a101"), ErrorCode::Unbalanced, 2);
    rejects(hex("8500"), ErrorCode::InvalidReportId, 0);
    rejects(hex("860100"), ErrorCode::InvalidItemSize, 0);
    rejects(hex("8100"), ErrorCode::EmptyReport, 0);
    rejects(hex("7508950181028501"), ErrorCode::MixedReportIds, 6);
    // Both multiplication and accumulated length must be bounded.
    rejects(hex("77ffffffff97ffffffff8102"), ErrorCode::ReportTooLarge, 10);
    rejects(hex("7508970000010081028102"), ErrorCode::ReportTooLarge, 9);
    rejects(std::vector<std::byte>(MaxDescriptorBytes + 1), ErrorCode::TooLarge, MaxDescriptorBytes);
    rejects(std::vector<std::byte>(MaxNesting + 1, std::byte{0xa4}), ErrorCode::NestingLimit, MaxNesting);
    std::vector<std::byte> nested;
    for (std::size_t i = 0; i <= MaxNesting; ++i) {
        nested.push_back(std::byte{0xa1});
        nested.push_back(std::byte{0x01});
    }
    rejects(nested, ErrorCode::NestingLimit, MaxNesting * 2);

    // All truncated prefixes of this captured, collection-wrapped descriptor
    // must fail without yielding partially trusted layouts.
    for (std::size_t size = 0; size < captured.size(); ++size) {
        const auto prefix = parse(std::span(captured).first(size));
        // Prefixes before the Collection item may be valid metadata only.
        if (size >= 7)
            check(!prefix, "unfinished collection cannot yield a valid layout");
    }

    // Reproducible adversarial corpus for sanitizer runs, not a claim of
    // exhaustive fuzz coverage. No reads/writes beyond the supplied span.
    std::uint32_t seed = 0x860;
    std::array<std::byte, 256> noise{};
    for (unsigned trial = 0; trial < 10000; ++trial) {
        for (auto &byte : noise) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            byte = static_cast<std::byte>(seed & 0xff);
        }
        const auto bytes = std::span(noise).first(trial % noise.size());
        const auto decoded = parse(bytes);
        if (!decoded)
            check(decoded.error().offset <= bytes.size(), "error offset stays in input");
        else {
            check(std::ranges::is_sorted(decoded->usagePages), "usage pages are sorted");
            check(std::adjacent_find(decoded->usagePages.begin(), decoded->usagePages.end()) ==
                      decoded->usagePages.end(),
                  "usage pages are unique");
            for (const auto &report : decoded->reports) {
                check(report.payloadBytes > 0 && report.payloadBytes <= MaxReportBits / 8,
                      "successful report lengths stay bounded");
                check(report.wireBytes() == report.payloadBytes + (report.id ? 1U : 0U),
                      "wire size includes exactly one numbered ID byte");
            }
        }
    }
    // Structured mutations reach deeper states than short random input alone.
    for (std::size_t index = 0; index < captured.size(); ++index) {
        for (unsigned value = 0; value < 256; ++value) {
            auto mutated = captured;
            mutated[index] = static_cast<std::byte>(value);
            const auto decoded = parse(mutated);
            if (!decoded)
                check(decoded.error().offset <= mutated.size(), "structured mutation error offset bounded");
            else
                for (const auto &report : decoded->reports)
                    check(report.payloadBytes > 0 && report.payloadBytes <= MaxReportBits / 8,
                          "structured mutation report bounded");
        }
    }
    std::puts("Typed HID parser: capture, malformed input, resource budgets and adversarial corpus passed.");
}
