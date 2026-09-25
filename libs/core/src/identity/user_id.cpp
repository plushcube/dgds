#include <dgds/core/identity/user_id.h>

#include <dgds/core/identity/content_identity.h>

#include <openssl/rand.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace dgds::core {
namespace {

constexpr std::uint8_t k_uuid_version = 4;
constexpr std::size_t k_version_offset = 6;
constexpr std::uint8_t k_version_mask = 0x0F;
constexpr std::size_t k_variant_offset = 8;
constexpr std::uint8_t k_variant_bits = 0xC0;
constexpr std::uint8_t k_rfc_variant = 0x80;

constexpr std::size_t k_text_size = 36;
constexpr std::size_t k_digit_count = 32;
constexpr std::size_t k_dash_after_digits[] = {8, 12, 16, 20};
constexpr std::size_t k_dash_at_text[] = {8, 13, 18, 23};

bool has_valid_version(const UserId &user_id) { return (user_id[k_version_offset] >> 4) == k_uuid_version; }

bool has_valid_variant(const UserId &user_id) { return (user_id[k_variant_offset] & k_variant_bits) == k_rfc_variant; }

} // namespace

Result<UserId> generate_user_id() {
  UserId user_id{};

  if (RAND_bytes(user_id.data(), static_cast<int>(user_id.size())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  user_id[k_version_offset] =
      static_cast<std::uint8_t>((user_id[k_version_offset] & k_version_mask) | (k_uuid_version << 4));
  user_id[k_variant_offset] = static_cast<std::uint8_t>(
      (user_id[k_variant_offset] & static_cast<std::uint8_t>(~k_variant_bits)) | k_rfc_variant);

  return user_id;
}

std::string to_uuid(const UserId &user_id) {
  const std::string digits = to_hex(user_id.data(), user_id.size());

  std::string text;
  text.reserve(k_text_size);

  std::size_t dash = 0;

  for (std::size_t index = 0; index < digits.size(); ++index) {
    if (dash < std::size(k_dash_after_digits) && index == k_dash_after_digits[dash]) {
      text.push_back('-');
      ++dash;
    }

    text.push_back(digits[index]);
  }

  return text;
}

std::optional<UserId> from_uuid(Content text) {
  if (text.size() != k_text_size) {
    return std::nullopt;
  }

  std::string digits;
  digits.reserve(k_digit_count);

  std::size_t dash = 0;

  for (std::size_t index = 0; index < text.size(); ++index) {
    if (dash < std::size(k_dash_at_text) && index == k_dash_at_text[dash]) {
      if (text[index] != '-') {
        return std::nullopt;
      }

      ++dash;
      continue;
    }

    digits.push_back(text[index]);
  }

  const auto decoded = from_hex(digits);

  if (!decoded.has_value() || decoded->size() != k_user_id_size) {
    return std::nullopt;
  }

  UserId user_id{};

  for (std::size_t index = 0; index < user_id.size(); ++index) {
    user_id[index] = static_cast<std::uint8_t>((*decoded)[index]);
  }

  if (!has_valid_version(user_id) || !has_valid_variant(user_id)) {
    return std::nullopt;
  }

  return user_id;
}

} // namespace dgds::core
