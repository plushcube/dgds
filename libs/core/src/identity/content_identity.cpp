#include <dgds/core/identity/content_identity.h>

#include <openssl/evp.h>

#include <cstdint>
#include <expected>

namespace dgds::core {
namespace {

std::optional<std::uint8_t> hex_digit(char symbol) {
  if (symbol >= '0' && symbol <= '9') {
    return static_cast<std::uint8_t>(symbol - '0');
  }

  if (symbol >= 'a' && symbol <= 'f') {
    return static_cast<std::uint8_t>(symbol - 'a' + 10);
  }

  return std::nullopt;
}

} // namespace

Content as_content(const ContentIdentity &identity) {
  return Content(reinterpret_cast<const char *>(identity.data()), identity.size());
}

Result<ContentIdentity> content_identity(Content content) {
  const CanonicalForm canonical = canonical_form(content);

  ContentIdentity identity{};

  unsigned int length = 0;
  const int status = EVP_Digest(canonical.data(), canonical.size(), identity.data(), &length, EVP_sha256(), nullptr);

  if (status != 1 || length != identity.size()) {
    return std::unexpected(CoreError::digest_failed);
  }

  return identity;
}

std::string to_hex(const std::uint8_t *bytes, std::size_t size) {
  constexpr char k_digits[] = "0123456789abcdef";

  std::string text;
  text.reserve(size * 2);

  for (std::size_t index = 0; index < size; ++index) {
    text.push_back(k_digits[bytes[index] >> 4]);
    text.push_back(k_digits[bytes[index] & 0x0F]);
  }

  return text;
}

std::string to_hex(const ContentIdentity &identity) { return to_hex(identity.data(), identity.size()); }

std::optional<ContentBuffer> from_hex(Content text) {
  if (text.size() % 2 != 0) {
    return std::nullopt;
  }

  ContentBuffer bytes;
  bytes.reserve(text.size() / 2);

  for (std::size_t index = 0; index < text.size(); index += 2) {
    const std::optional<std::uint8_t> high = hex_digit(text[index]);
    const std::optional<std::uint8_t> low = hex_digit(text[index + 1]);

    if (!high.has_value() || !low.has_value()) {
      return std::nullopt;
    }

    bytes.push_back(static_cast<char>((high.value() << 4) | low.value()));
  }

  return bytes;
}

} // namespace dgds::core
