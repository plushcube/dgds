#pragma once

#include <dgds/core/models/content.h>

#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr char32_t k_unknown_code_point = 0xFFFFFFFF;

struct DecodedCodePoint {
  char32_t code_point;
  std::size_t length;
};

[[nodiscard]] DecodedCodePoint decode_utf8(Content content, std::size_t offset);

} // namespace dgds::core
