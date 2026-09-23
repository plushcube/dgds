#pragma once

#include <dgds/core/models/purchase.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::uint8_t k_mark_version = 1;
inline constexpr std::size_t k_mark_bit_count = 88;
inline constexpr std::size_t k_mark_confidence_threshold = 2;
inline constexpr char32_t k_mark_code_point = 0x200B;

using MarkBits = std::array<std::uint8_t, k_mark_bit_count>;

struct Mark {
  PurchaseId purchase_id;
  std::uint8_t version;

  bool operator==(const Mark &) const = default;
};

} // namespace dgds::core
