#pragma once

#include <dgds/core/models/aead_algorithm.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dgds::core {

inline constexpr std::size_t k_nonce_size = 12;
inline constexpr std::size_t k_tag_size = 16;

using Nonce = std::array<std::uint8_t, k_nonce_size>;
using Tag = std::array<std::uint8_t, k_tag_size>;
using Ciphertext = std::vector<std::uint8_t>;

struct SealedContent {
  AeadAlgorithm algorithm;
  Nonce nonce;
  Ciphertext ciphertext;
  Tag tag;
};

} // namespace dgds::core
