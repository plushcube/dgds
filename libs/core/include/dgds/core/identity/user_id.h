#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/user.h>

#include <optional>
#include <string>

namespace dgds::core {

[[nodiscard]] Result<UserId> generate_user_id();
[[nodiscard]] std::string to_uuid(const UserId &user_id);
[[nodiscard]] std::optional<UserId> from_uuid(Content text);

} // namespace dgds::core
