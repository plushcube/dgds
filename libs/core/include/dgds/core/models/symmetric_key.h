#pragma once

#include <dgds/core/models/secret_bytes.h>

#include <cstddef>

namespace dgds::core {

inline constexpr std::size_t k_key_size = 32;

using SymmetricKey = SecretBytes<k_key_size>;

} // namespace dgds::core
