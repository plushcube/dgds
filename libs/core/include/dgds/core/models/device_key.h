#pragma once

#include <dgds/core/models/secret_bytes.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_device_key_size = 32;

using DevicePublicKey = std::array<std::uint8_t, k_device_key_size>;
using DevicePrivateKey = SecretBytes<k_device_key_size>;

struct DeviceKeyPair {
  DevicePublicKey public_key;
  DevicePrivateKey private_key;
};

} // namespace dgds::core
