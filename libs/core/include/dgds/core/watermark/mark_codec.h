#pragma once

#include <dgds/core/models/errors.h>
#include <dgds/core/models/mark.h>

namespace dgds::core {

[[nodiscard]] MarkBits encode_mark(const Mark &mark);
[[nodiscard]] Result<Mark> decode_mark(const MarkBits &bits);

} // namespace dgds::core
