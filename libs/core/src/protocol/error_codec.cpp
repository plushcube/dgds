#include <dgds/core/models/protocol.h>

#include <optional>
#include <string_view>
#include <utility>

namespace dgds::core {
namespace {

using CodeEntry = std::pair<CoreError, std::string_view>;

constexpr CodeEntry k_codes[]{
    {CoreError::digest_failed, "digest_failed"},
    {CoreError::storage_failed, "storage_failed"},
    {CoreError::key_not_found, "key_not_found"},
    {CoreError::key_malformed, "key_malformed"},
    {CoreError::key_size_mismatch, "key_size_mismatch"},
    {CoreError::algorithm_unsupported, "algorithm_unsupported"},
    {CoreError::crypto_failed, "crypto_failed"},
    {CoreError::authentication_failed, "authentication_failed"},
    {CoreError::sealed_content_malformed, "sealed_content_malformed"},
    {CoreError::blob_not_found, "blob_not_found"},
    {CoreError::blob_malformed, "blob_malformed"},
    {CoreError::user_not_found, "user_not_found"},
    {CoreError::user_name_taken, "user_name_taken"},
    {CoreError::authorization_failed, "authorization_failed"},
    {CoreError::not_permitted, "not_permitted"},
    {CoreError::publication_not_found, "publication_not_found"},
    {CoreError::content_duplicate, "content_duplicate"},
    {CoreError::purchase_not_found, "purchase_not_found"},
    {CoreError::record_exists, "record_exists"},
    {CoreError::receipt_version_unsupported, "receipt_version_unsupported"},
    {CoreError::receipt_not_found, "receipt_not_found"},
    {CoreError::receipt_malformed, "receipt_malformed"},
    {CoreError::package_version_unsupported, "package_version_unsupported"},
    {CoreError::content_mismatch, "content_mismatch"},
    {CoreError::signature_invalid, "signature_invalid"},
    {CoreError::mark_malformed, "mark_malformed"},
    {CoreError::mark_version_unsupported, "mark_version_unsupported"},
    {CoreError::mark_checksum_mismatch, "mark_checksum_mismatch"},
    {CoreError::mark_not_found, "mark_not_found"},
    {CoreError::mark_not_confident, "mark_not_confident"},
    {CoreError::connection_failed, "connection_failed"},
    {CoreError::protocol_version_unsupported, "protocol_version_unsupported"},
    {CoreError::protocol_failure, "protocol_failure"},
    {CoreError::rate_limit_exceeded, "rate_limit_exceeded"},
};

} // namespace

std::string_view code_of(CoreError error) {
  for (const CodeEntry &entry : k_codes) {
    if (entry.first == error) {
      return entry.second;
    }
  }

  return k_failure_code;
}

std::optional<CoreError> error_of_code(std::string_view code) {
  for (const CodeEntry &entry : k_codes) {
    if (entry.second == code) {
      return entry.first;
    }
  }

  return std::nullopt;
}

} // namespace dgds::core
