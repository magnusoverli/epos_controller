#pragma once
#include <string_view>

// Synthetic predictions from the adapter's Service_FactoryReset serializer,
// not live reset captures. See fixtures/settings/PROMPTS.md.
namespace DerivedActions {
inline constexpr std::string_view ResetRequest = "01ebfe0100000001000400af0001001300";
inline constexpr std::string_view ResetReply = "01ebfe0100000000000400af0001001300";
} // namespace DerivedActions
