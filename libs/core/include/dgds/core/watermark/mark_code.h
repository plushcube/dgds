#pragma once

#include <dgds/core/models/crypto.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/identity.h>
#include <dgds/core/models/mark.h>

#include <cstdint>

namespace dgds::core {

[[nodiscard]] Result<MarkCode> mark_code(const SymmetricKey &mark_key, const ContentIdentity &identity,
                                         PurchaseId purchase_id, std::uint8_t version);
[[nodiscard]] Result<bool> verify_mark_code(const SymmetricKey &mark_key, const ContentIdentity &identity,
                                            const Mark &mark);

} // namespace dgds::core
