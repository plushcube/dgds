#pragma once

#include <dgds/core/models/claim_outcome.h>
#include <dgds/core/models/content_identity.h>
#include <dgds/core/models/result.h>

namespace dgds::core {

class IdentityRegistry {
public:
  IdentityRegistry() = default;
  IdentityRegistry(const IdentityRegistry &) = delete;
  IdentityRegistry &operator=(const IdentityRegistry &) = delete;
  IdentityRegistry(IdentityRegistry &&) = delete;
  IdentityRegistry &operator=(IdentityRegistry &&) = delete;
  virtual ~IdentityRegistry() = default;

  [[nodiscard]] virtual Result<ClaimOutcome> claim(const ContentIdentity &identity) = 0;
};

} // namespace dgds::core
