#include <dgds/core/identity/content_identity.h>

#include <openssl/evp.h>

#include <cstdint>
#include <expected>

namespace dgds::core {

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

std::string to_hex(const ContentIdentity &identity) {
  constexpr char k_digits[] = "0123456789abcdef";

  std::string text;
  text.reserve(identity.size() * 2);

  for (const std::uint8_t byte : identity) {
    text.push_back(k_digits[byte >> 4]);
    text.push_back(k_digits[byte & 0x0F]);
  }

  return text;
}

} // namespace dgds::core
