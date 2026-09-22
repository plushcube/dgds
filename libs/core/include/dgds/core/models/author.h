#pragma once

#include <dgds/core/models/crypto.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_author_public_key_size = 32;
inline constexpr std::size_t k_author_private_key_size = 32;
inline constexpr std::size_t k_signature_size = 64;

enum class SignatureAlgorithm : std::uint8_t {
  ed25519 = 1,
};

using AuthorPublicKey = std::array<std::uint8_t, k_author_public_key_size>;
using AuthorPrivateKey = SecretBytes<k_author_private_key_size>;
using Signature = std::array<std::uint8_t, k_signature_size>;

struct AuthorKeyPair {
  AuthorPublicKey public_key;
  AuthorPrivateKey private_key;
};

} // namespace dgds::core
