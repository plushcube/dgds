#pragma once

#include <dgds/core/identity/content_identity.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace dgds::client {

inline constexpr std::size_t k_server_pin_size = 32;

using ServerPinBytes = std::array<std::uint8_t, k_server_pin_size>;

class ServerPin {
public:
  ServerPin() = default;

  explicit ServerPin(ServerPinBytes bytes) : m_bytes(bytes) {}

  [[nodiscard]] const ServerPinBytes &bytes() const { return m_bytes; }

  [[nodiscard]] std::string to_hex() const { return core::to_hex(m_bytes.data(), m_bytes.size()); }

  [[nodiscard]] friend bool operator==(const ServerPin &left, const ServerPin &right) = default;

private:
  ServerPinBytes m_bytes{};
};

} // namespace dgds::client
