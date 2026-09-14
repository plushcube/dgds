#pragma once

#include <dgds/core/models/mark.h>
#include <dgds/core/models/result.h>

namespace dgds::core {

[[nodiscard]] MarkBits encode_mark(const Mark &mark);
[[nodiscard]] Result<Mark> decode_mark(const MarkBits &bits);

} // namespace dgds::core
