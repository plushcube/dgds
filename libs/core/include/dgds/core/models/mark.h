#pragma once

#include <dgds/core/models/purchase.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::uint8_t k_mark_version = 1;
inline constexpr std::size_t k_mark_bit_count = 88;

using MarkBits = std::array<std::uint8_t, k_mark_bit_count>;

struct Mark {
  PurchaseId purchase_id;
  std::uint8_t version;

  bool operator==(const Mark &) const = default;
};

} // namespace dgds::core
