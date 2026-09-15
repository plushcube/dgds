#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_key_size = 32;

using SymmetricKey = std::array<std::uint8_t, k_key_size>;

} // namespace dgds::core
