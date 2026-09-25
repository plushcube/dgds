#pragma once

#include <expected>

namespace dgds::core {

enum class CoreError {
  digest_failed,
  storage_failed,
  key_not_found,
  key_malformed,
  key_size_mismatch,
  algorithm_unsupported,
  crypto_failed,
  authentication_failed,
  sealed_content_malformed,
  blob_not_found,
  blob_malformed,
  user_not_found,
  user_name_taken,
  authorization_failed,
  not_permitted,
  publication_not_found,
  content_duplicate,
  purchase_not_found,
  record_exists,
  receipt_version_unsupported,
  receipt_not_found,
  receipt_malformed,
  package_version_unsupported,
  content_mismatch,
  signature_invalid,
  mark_malformed,
  mark_version_unsupported,
  mark_checksum_mismatch,
  mark_not_found,
  mark_not_confident,
  connection_failed,
  protocol_version_unsupported,
  protocol_failure,
  rate_limit_exceeded,
};

template <typename Value> using Result = std::expected<Value, CoreError>;

} // namespace dgds::core
