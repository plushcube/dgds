#include <dgds/core/identity/identifier.h>

#include <dgds/core/codec/binary.h>

#include <openssl/rand.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace dgds::core {
namespace {

constexpr std::size_t k_identifier_size = sizeof(std::uint64_t);

} // namespace

Result<std::uint64_t> generate_identifier() {
  std::array<std::uint8_t, k_identifier_size> bytes{};

  if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  std::uint64_t identifier = 0;

  for (const std::uint8_t byte : bytes) {
    identifier = (identifier << k_bits_per_byte) | byte;
  }

  return identifier;
}

} // namespace dgds::core
