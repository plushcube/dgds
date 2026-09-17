#pragma once

#include <cstdint>

namespace dgds::core {

enum class AeadAlgorithm : std::uint8_t {
  aes_256_gcm = 1,
};

} // namespace dgds::core
