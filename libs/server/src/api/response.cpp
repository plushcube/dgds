#include <dgds/server/api/response.h>

#include <dgds/core/models/protocol.h>

#include <nlohmann/json.hpp>

#include <string>

namespace dgds::server::api {
namespace {

std::string error_envelope(core::Content code) {
  return nlohmann::json{{"version", core::k_protocol_version}, {"error", {{"code", std::string(code)}}}}.dump();
}

} // namespace

std::string ok(core::Content data) {
  const nlohmann::json value = nlohmann::json::parse(data, nullptr, false);

  if (value.is_discarded()) {
    return failure(ProtocolError::response_malformed);
  }

  return nlohmann::json{{"version", core::k_protocol_version}, {"data", value}}.dump();
}

std::string failure(ProtocolError error) { return error_envelope(code_of(error)); }

std::string failure(core::CoreError error) { return error_envelope(code_of(error)); }

} // namespace dgds::server::api
