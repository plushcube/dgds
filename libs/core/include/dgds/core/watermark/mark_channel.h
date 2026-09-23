#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/mark.h>

#include <optional>

namespace dgds::core {

[[nodiscard]] bool has_mark_channel(Content text);
[[nodiscard]] std::optional<ContentBuffer> embed_mark(Content text, const Mark &mark);
[[nodiscard]] Result<Mark> read_mark(Content text);

} // namespace dgds::core
