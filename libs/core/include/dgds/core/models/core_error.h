#pragma once

namespace dgds::core {

enum class CoreError {
  digest_failed,
  storage_failed,
  algorithm_unsupported,
  crypto_failed,
  authentication_failed,
  key_size_mismatch,
  mark_malformed,
  mark_version_unsupported,
  mark_checksum_mismatch,
};

} // namespace dgds::core
