#pragma once

#include <dgds/core/models/purchase.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::uint8_t k_mark_version = 2;
inline constexpr std::size_t k_mark_code_size = 8;
inline constexpr std::size_t k_mark_bit_count = 136;
inline constexpr std::size_t k_mark_confidence_threshold = 2;
inline constexpr char32_t k_mark_code_point = 0x200B;

using MarkCode = std::array<std::uint8_t, k_mark_code_size>;
using MarkBits = std::array<std::uint8_t, k_mark_bit_count>;

struct Mark {
  PurchaseId purchase_id;
  MarkCode code;
  std::uint8_t version;

  bool operator==(const Mark &) const = default;
};

} // namespace dgds::core
