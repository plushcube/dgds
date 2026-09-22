#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/errors.h>

namespace dgds::core {

[[nodiscard]] Result<SymmetricKey> generate_key();
[[nodiscard]] Result<SealedContent> wrap_key(const SymmetricKey &key, const SymmetricKey &wrapping_key,
                                             Content associated_data);
[[nodiscard]] Result<SymmetricKey> unwrap_key(const SealedContent &wrapped, const SymmetricKey &wrapping_key,
                                              Content associated_data);

} // namespace dgds::core
