#include <dgds/server/api/errors.h>

#include <dgds/core/models/protocol.h>

namespace dgds::server::api {

core::Content code_of(ProtocolError error) {
  switch (error) {
  case ProtocolError::request_malformed:
    return core::k_request_malformed_code;
  case ProtocolError::version_unsupported:
    return core::code_of(core::CoreError::protocol_version_unsupported);
  case ProtocolError::operation_unknown:
    return core::k_operation_unknown_code;
  case ProtocolError::response_malformed:
    return core::k_response_malformed_code;
  case ProtocolError::rate_limit_exceeded:
    return core::code_of(core::CoreError::rate_limit_exceeded);
  }

  return core::k_failure_code;
}

} // namespace dgds::server::api
