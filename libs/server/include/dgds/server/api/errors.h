#pragma once

#include <dgds/core/models/content.h>

namespace dgds::server::api {

enum class ProtocolError {
  request_malformed,
  version_unsupported,
  operation_unknown,
  response_malformed,
  rate_limit_exceeded,
};

[[nodiscard]] core::Content code_of(ProtocolError error);

} // namespace dgds::server::api
