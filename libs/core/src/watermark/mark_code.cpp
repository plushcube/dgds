#include <dgds/core/watermark/mark_code.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>

namespace dgds::core {
namespace {

using Mac = std::unique_ptr<EVP_MAC, decltype(&EVP_MAC_free)>;
using MacContext = std::unique_ptr<EVP_MAC_CTX, decltype(&EVP_MAC_CTX_free)>;

constexpr const char k_digest[] = "SHA256";
constexpr std::size_t k_bits_per_byte = 8;
constexpr std::size_t k_digest_size = 32;
constexpr std::size_t k_id_size = sizeof(PurchaseId);

using Message = std::array<std::uint8_t, 1 + k_id_size + k_identity_size>;

Message to_message(const ContentIdentity &identity, PurchaseId purchase_id, std::uint8_t version) {
  Message message{};
  message[0] = version;

  for (std::size_t index = 0; index < k_id_size; ++index) {
    const std::size_t shift = (k_id_size - 1 - index) * k_bits_per_byte;
    message[1 + index] = static_cast<std::uint8_t>(purchase_id >> shift);
  }

  std::copy(identity.begin(), identity.end(), message.begin() + static_cast<std::ptrdiff_t>(1 + k_id_size));

  return message;
}

} // namespace

Result<MarkCode> mark_code(const SymmetricKey &mark_key, const ContentIdentity &identity, PurchaseId purchase_id,
                           std::uint8_t version) {
  const Message message = to_message(identity, purchase_id, version);

  const Mac mac(EVP_MAC_fetch(nullptr, "HMAC", nullptr), &EVP_MAC_free);

  if (!mac) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const MacContext context(EVP_MAC_CTX_new(mac.get()), &EVP_MAC_CTX_free);

  if (!context) {
    return std::unexpected(CoreError::crypto_failed);
  }

  OSSL_PARAM params[] = {OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, const_cast<char *>(k_digest), 0),
                         OSSL_PARAM_construct_end()};

  if (EVP_MAC_init(context.get(), mark_key.data(), mark_key.size(), params) != 1 ||
      EVP_MAC_update(context.get(), message.data(), message.size()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  std::array<std::uint8_t, k_digest_size> digest{};
  std::size_t digest_size = digest.size();

  if (EVP_MAC_final(context.get(), digest.data(), &digest_size, digest.size()) != 1 || digest_size != digest.size()) {
    return std::unexpected(CoreError::crypto_failed);
  }

  MarkCode code{};
  std::copy(digest.begin(), digest.begin() + static_cast<std::ptrdiff_t>(code.size()), code.begin());

  return code;
}

Result<bool> verify_mark_code(const SymmetricKey &mark_key, const ContentIdentity &identity, const Mark &mark) {
  const auto expected = mark_code(mark_key, identity, mark.purchase_id, mark.version);

  if (!expected.has_value()) {
    return std::unexpected(expected.error());
  }

  return CRYPTO_memcmp(expected->data(), mark.code.data(), mark.code.size()) == 0;
}

} // namespace dgds::core
