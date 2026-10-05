#include <dgds/core/watermark/mark_key.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>

namespace dgds::core {
namespace {

using Kdf = std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)>;
using KdfContext = std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)>;

constexpr const char k_digest[] = "SHA256";
constexpr const char k_derivation_info[] = "dgds-mark-key-v1";

} // namespace

Result<SymmetricKey> derive_mark_key(const SymmetricKey &master_key) {
  const Kdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr), &EVP_KDF_free);

  if (!kdf) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const KdfContext context(EVP_KDF_CTX_new(kdf.get()), &EVP_KDF_CTX_free);

  if (!context) {
    return std::unexpected(CoreError::crypto_failed);
  }

  SymmetricKey mark_key{};

  OSSL_PARAM params[] = {OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char *>(k_digest), 0),
                         OSSL_PARAM_construct_octet_string(
                             OSSL_KDF_PARAM_KEY, const_cast<std::uint8_t *>(master_key.data()), master_key.size()),
                         OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, const_cast<char *>(k_derivation_info),
                                                           sizeof(k_derivation_info) - 1),
                         OSSL_PARAM_construct_end()};

  if (EVP_KDF_derive(context.get(), mark_key.data(), mark_key.size(), params) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return mark_key;
}

} // namespace dgds::core
