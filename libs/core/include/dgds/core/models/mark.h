#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

using PurchaseId = std::uint64_t;

inline constexpr std::uint8_t k_mark_version = 1;
inline constexpr std::size_t k_mark_bit_count = 88;

using MarkBits = std::array<std::uint8_t, k_mark_bit_count>;

struct Mark {
  PurchaseId purchase_id;
  std::uint8_t version;

  bool operator==(const Mark &) const = default;
};

} // namespace dgds::core
