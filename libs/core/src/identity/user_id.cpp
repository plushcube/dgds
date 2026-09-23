#include <dgds/core/identity/user_id.h>

#include <openssl/rand.h>

#include <cstddef>
#include <cstdint>
#include <expected>

namespace dgds::core {
namespace {

constexpr std::uint8_t k_uuid_version = 4;
constexpr std::size_t k_version_offset = 6;
constexpr std::uint8_t k_version_mask = 0x0F;
constexpr std::size_t k_variant_offset = 8;
constexpr std::uint8_t k_variant_mask = 0x3F;
constexpr std::uint8_t k_rfc_variant = 0x80;

} // namespace

Result<UserId> generate_user_id() {
  UserId user_id{};

  if (RAND_bytes(user_id.data(), static_cast<int>(user_id.size())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  user_id[k_version_offset] =
      static_cast<std::uint8_t>((user_id[k_version_offset] & k_version_mask) | (k_uuid_version << 4));
  user_id[k_variant_offset] = static_cast<std::uint8_t>((user_id[k_variant_offset] & k_variant_mask) | k_rfc_variant);

  return user_id;
}

} // namespace dgds::core
