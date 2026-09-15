#include <dgds/stubs/identity_registry/file_identity_registry.h>

#include <dgds/core/identity/content_identity.h>

#include <cerrno>
#include <expected>
#include <fcntl.h>
#include <unistd.h>

namespace dgds::stubs {
namespace {

constexpr const char k_claim_suffix[] = ".claimed";

core::Result<core::ClaimOutcome> claim_file(const std::filesystem::path &path) {
  const int descriptor = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);

  if (descriptor >= 0) {
    ::close(descriptor);
    return core::ClaimOutcome::claimed;
  }

  if (errno == EEXIST) {
    return core::ClaimOutcome::already_claimed;
  }

  return std::unexpected(core::CoreError::storage_failed);
}

} // namespace

core::Result<core::ClaimOutcome> FileIdentityRegistry::claim(const core::ContentIdentity &identity) {
  return claim_file(m_root / (core::to_hex(identity) + k_claim_suffix));
}

} // namespace dgds::stubs
