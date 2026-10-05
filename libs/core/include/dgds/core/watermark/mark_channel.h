#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/mark.h>

#include <optional>
#include <vector>

namespace dgds::core {

[[nodiscard]] bool is_channel_domain(Content text);
[[nodiscard]] bool has_mark_channel(Content text);
[[nodiscard]] std::optional<ContentBuffer> embed_mark(Content text, const Mark &mark);
[[nodiscard]] Result<std::vector<Mark>> read_marks(Content text);

} // namespace dgds::core
