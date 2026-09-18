#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/user_id.h>

namespace dgds::core {

struct UserAccount {
  UserId user_id;
  ContentBuffer name;
};

} // namespace dgds::core
