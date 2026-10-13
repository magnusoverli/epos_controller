#include "HidDescriptor.h"
#include <algorithm>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, std::size_t size) {
    const auto result = HidDescriptor::parse(std::as_bytes(std::span(data, size)));
    if (!result) {
        if (result.error().offset > size)
            std::abort();
        return 0;
    }
    if (!std::ranges::is_sorted(result->usagePages))
        std::abort();
    for (const auto &report : result->reports) {
        if (report.payloadBytes == 0 || report.payloadBytes > HidDescriptor::MaxReportBits / 8)
            std::abort();
        if (report.wireBytes() != report.payloadBytes + (report.id ? 1U : 0U))
            std::abort();
    }
    return 0;
}
