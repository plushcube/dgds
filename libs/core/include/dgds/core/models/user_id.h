#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_user_id_size = 16;

using UserId = std::array<std::uint8_t, k_user_id_size>;

} // namespace dgds::core
