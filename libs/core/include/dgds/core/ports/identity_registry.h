#pragma once

#include <dgds/core/models/errors.h>
#include <dgds/core/models/identity.h>

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
  [[nodiscard]] virtual Result<void> release(const ContentIdentity &identity) = 0;
};

} // namespace dgds::core
