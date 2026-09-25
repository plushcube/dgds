#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/server/api/errors.h>

#include <string>

namespace dgds::server::api {

struct SurfaceResult {
  bool accepted = false;
  core::Content code;
  std::string body;
};

[[nodiscard]] SurfaceResult ok(core::Content data);
[[nodiscard]] SurfaceResult failure(ProtocolError error);
[[nodiscard]] SurfaceResult failure(core::CoreError error);

} // namespace dgds::server::api
