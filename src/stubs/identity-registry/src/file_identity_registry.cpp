#include <dgds/stubs/identity_registry/file_identity_registry.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/stubs/support/file_storage.h>

#include <expected>

namespace dgds::stubs {
namespace {

constexpr const char k_claim_suffix[] = ".claimed";

} // namespace

core::Result<core::ClaimOutcome> FileIdentityRegistry::claim(const core::ContentIdentity &identity) {
  const auto claimed = claim_file(m_root / (core::to_hex(identity) + k_claim_suffix));

  if (!claimed.has_value()) {
    return std::unexpected(claimed.error());
  }

  return claimed.value() ? core::ClaimOutcome::claimed : core::ClaimOutcome::already_claimed;
}

core::Result<void> FileIdentityRegistry::release(const core::ContentIdentity &identity) {
  return remove_file(m_root / (core::to_hex(identity) + k_claim_suffix));
}

} // namespace dgds::stubs
