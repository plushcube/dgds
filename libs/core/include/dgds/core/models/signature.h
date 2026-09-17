#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_signature_size = 64;

using Signature = std::array<std::uint8_t, k_signature_size>;

} // namespace dgds::core
