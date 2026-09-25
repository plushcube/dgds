#include <dgds/server/api/response.h>

#include <dgds/core/models/protocol.h>

#include <nlohmann/json.hpp>

#include <string>

namespace dgds::server::api {

core::Content code_of(core::CoreError error) {
  switch (error) {
  case core::CoreError::digest_failed:
    return "digest_failed";
  case core::CoreError::storage_failed:
    return "storage_failed";
  case core::CoreError::key_not_found:
    return "key_not_found";
  case core::CoreError::key_malformed:
    return "key_malformed";
  case core::CoreError::key_size_mismatch:
    return "key_size_mismatch";
  case core::CoreError::algorithm_unsupported:
    return "algorithm_unsupported";
  case core::CoreError::crypto_failed:
    return "crypto_failed";
  case core::CoreError::authentication_failed:
    return "authentication_failed";
  case core::CoreError::sealed_content_malformed:
    return "sealed_content_malformed";
  case core::CoreError::blob_not_found:
    return "blob_not_found";
  case core::CoreError::blob_malformed:
    return "blob_malformed";
  case core::CoreError::user_not_found:
    return "user_not_found";
  case core::CoreError::user_name_taken:
    return "user_name_taken";
  case core::CoreError::authorization_failed:
    return "authorization_failed";
  case core::CoreError::not_permitted:
    return "not_permitted";
  case core::CoreError::publication_not_found:
    return "publication_not_found";
  case core::CoreError::content_duplicate:
    return "content_duplicate";
  case core::CoreError::purchase_not_found:
    return "purchase_not_found";
  case core::CoreError::record_exists:
    return "record_exists";
  case core::CoreError::receipt_version_unsupported:
    return "receipt_version_unsupported";
  case core::CoreError::receipt_not_found:
    return "receipt_not_found";
  case core::CoreError::receipt_malformed:
    return "receipt_malformed";
  case core::CoreError::package_version_unsupported:
    return "package_version_unsupported";
  case core::CoreError::content_mismatch:
    return "content_mismatch";
  case core::CoreError::signature_invalid:
    return "signature_invalid";
  case core::CoreError::mark_malformed:
    return "mark_malformed";
  case core::CoreError::mark_version_unsupported:
    return "mark_version_unsupported";
  case core::CoreError::mark_checksum_mismatch:
    return "mark_checksum_mismatch";
  case core::CoreError::mark_not_found:
    return "mark_not_found";
  case core::CoreError::mark_not_confident:
    return "mark_not_confident";
  }

  return "failure";
}

std::string ok(core::Content data) {
  const nlohmann::json value = nlohmann::json::parse(data, nullptr, false);

  if (value.is_discarded()) {
    return failure("response_malformed");
  }

  return nlohmann::json{{"version", core::k_protocol_version}, {"data", value}}.dump();
}

std::string failure(core::Content code) {
  return nlohmann::json{{"version", core::k_protocol_version}, {"error", {{"code", std::string(code)}}}}.dump();
}

std::string failure(core::CoreError error) { return failure(code_of(error)); }

} // namespace dgds::server::api
