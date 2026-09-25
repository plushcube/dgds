#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/server/api/errors.h>

#include <string>

namespace dgds::server::api {

[[nodiscard]] std::string ok(core::Content data);
[[nodiscard]] std::string failure(ProtocolError error);
[[nodiscard]] std::string failure(core::CoreError error);

} // namespace dgds::server::api
