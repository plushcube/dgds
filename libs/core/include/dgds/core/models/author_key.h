#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_author_public_key_size = 32;
inline constexpr std::size_t k_author_private_key_size = 32;

using AuthorPublicKey = std::array<std::uint8_t, k_author_public_key_size>;
using AuthorPrivateKey = std::array<std::uint8_t, k_author_private_key_size>;

struct AuthorKeyPair {
  AuthorPublicKey public_key;
  AuthorPrivateKey private_key;
};

} // namespace dgds::core
