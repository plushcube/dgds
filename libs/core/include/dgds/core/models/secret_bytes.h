#pragma once

#include <openssl/crypto.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

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

} // namespace dgds::core
