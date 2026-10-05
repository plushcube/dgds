#pragma once

#include <dgds/core/models/mark.h>

namespace dgds::core {

[[nodiscard]] MarkBits encode_mark(const Mark &mark);
[[nodiscard]] Mark decode_mark(const MarkBits &bits);
[[nodiscard]] bool is_legacy_mark(const MarkBits &bits);

} // namespace dgds::core
