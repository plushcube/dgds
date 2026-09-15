#pragma once

#include <cstdint>

namespace dgds::core {

enum class SignatureAlgorithm : std::uint8_t {
  ed25519 = 1,
};

} // namespace dgds::core
