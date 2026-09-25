#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>

#include <string>

namespace dgds::server::api {

[[nodiscard]] std::string ok(core::Content data);
[[nodiscard]] std::string failure(core::Content code);
[[nodiscard]] std::string failure(core::CoreError error);
[[nodiscard]] core::Content code_of(core::CoreError error);

} // namespace dgds::server::api
