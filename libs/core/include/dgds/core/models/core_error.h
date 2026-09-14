#pragma once

namespace dgds::core {

enum class CoreError {
  digest_failed,
  mark_malformed,
  mark_version_unsupported,
  mark_checksum_mismatch,
};

} // namespace dgds::core
