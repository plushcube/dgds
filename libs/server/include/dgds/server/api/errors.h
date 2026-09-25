#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>

namespace dgds::server::api {

enum class ProtocolError {
  request_malformed,
  version_unsupported,
  operation_unknown,
  response_malformed,
};

[[nodiscard]] core::Content code_of(ProtocolError error);
[[nodiscard]] core::Content code_of(core::CoreError error);

} // namespace dgds::server::api
