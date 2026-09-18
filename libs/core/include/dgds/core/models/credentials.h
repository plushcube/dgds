#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/user_id.h>

namespace dgds::core {

struct Credentials {
  UserId user_id;
  ContentBuffer token;
};

} // namespace dgds::core
