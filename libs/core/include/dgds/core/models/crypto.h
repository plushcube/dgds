#pragma once

#include <openssl/crypto.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dgds::core {

inline constexpr std::size_t k_key_size = 32;
inline constexpr std::size_t k_nonce_size = 12;
inline constexpr std::size_t k_tag_size = 16;

enum class AeadAlgorithm : std::uint8_t {
  aes_256_gcm = 1,
};

template <std::size_t Size> class SecretBytes {
public:
  SecretBytes() = default;
  SecretBytes(const SecretBytes &) = default;
  SecretBytes &operator=(const SecretBytes &) = default;

  SecretBytes(SecretBytes &&other) noexcept : m_bytes(other.m_bytes) { other.wipe(); }

  SecretBytes &operator=(SecretBytes &&other) noexcept {
    if (this != &other) {
      m_bytes = other.m_bytes;
      other.wipe();
    }

    return *this;
  }

  ~SecretBytes() { wipe(); }

  void wipe() { OPENSSL_cleanse(m_bytes.data(), m_bytes.size()); }

  [[nodiscard]] bool equals(const SecretBytes &other) const {
    return CRYPTO_memcmp(m_bytes.data(), other.m_bytes.data(), m_bytes.size()) == 0;
  }

  [[nodiscard]] std::uint8_t *data() { return m_bytes.data(); }
  [[nodiscard]] const std::uint8_t *data() const { return m_bytes.data(); }
  [[nodiscard]] std::size_t size() const { return m_bytes.size(); }

private:
  std::array<std::uint8_t, Size> m_bytes{};
};

using SymmetricKey = SecretBytes<k_key_size>;
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
