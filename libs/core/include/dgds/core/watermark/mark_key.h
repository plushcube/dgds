#pragma once

#include <dgds/core/models/crypto.h>
#include <dgds/core/models/errors.h>

namespace dgds::core {

[[nodiscard]] Result<SymmetricKey> derive_mark_key(const SymmetricKey &master_key);

} // namespace dgds::core
