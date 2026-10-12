#pragma once

#include <QString>
#include <QVariantList>

namespace Diagnostics {
// Serialization only: no refresh, clipboard or filesystem side effects.
[[nodiscard]] QString toJson(const QVariantList &devices);
}
