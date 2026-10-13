#pragma once

#include "HidDescriptor.h"
#include <QVariantMap>

namespace HidDescriptorPresentation {
// Qt/JSON compatibility belongs at the boundary, not in the byte parser.
[[nodiscard]] QVariantMap toVariant(const HidDescriptor::Result &result, std::size_t descriptorBytes);
} // namespace HidDescriptorPresentation
