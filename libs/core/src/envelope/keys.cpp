#include <dgds/core/envelope/keys.h>

#include <dgds/core/crypto/aead.h>

#include <openssl/rand.h>

#include <algorithm>
#include <cstddef>
#include <expected>

namespace dgds::core {
namespace {

Content as_content(const SymmetricKey &key) { return Content(reinterpret_cast<const char *>(key.data()), key.size()); }

} // namespace

Result<SymmetricKey> generate_key() {
  SymmetricKey key{};

  if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return key;
}

Result<SealedContent> wrap_key(const SymmetricKey &key, const SymmetricKey &wrapping_key, Content associated_data) {
  return encrypt(as_content(key), wrapping_key, associated_data);
}

Result<SymmetricKey> unwrap_key(const SealedContent &wrapped, const SymmetricKey &wrapping_key,
                                Content associated_data) {
  const auto plaintext = decrypt(wrapped, wrapping_key, associated_data);

  if (!plaintext.has_value()) {
    return std::unexpected(plaintext.error());
  }

  if (plaintext->size() != k_key_size) {
    return std::unexpected(CoreError::key_size_mismatch);
  }

  SymmetricKey key{};
  std::copy(plaintext->data(), plaintext->data() + plaintext->size(), key.data());

  return key;
}

} // namespace dgds::core
