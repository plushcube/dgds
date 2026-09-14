#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_identity_size = 32;

using ContentIdentity = std::array<std::uint8_t, k_identity_size>;

} // namespace dgds::core
