#pragma once

#include <dgds/core/models/errors.h>
#include <dgds/core/models/user.h>

namespace dgds::core {

[[nodiscard]] Result<UserId> generate_user_id();

} // namespace dgds::core
