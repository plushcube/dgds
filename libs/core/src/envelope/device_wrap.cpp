#include <dgds/core/envelope/device_wrap.h>

#include <dgds/core/envelope/keys.h>
#include <dgds/core/models/secret_bytes.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>

#include <cstddef>
#include <expected>
#include <memory>

namespace dgds::core {
namespace {

using Pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PkeyContext = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using Kdf = std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)>;
using KdfContext = std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)>;

constexpr const char k_digest[] = "SHA256";
constexpr const char k_derivation_info[] = "dgds-device-envelope-v1";

Pkey make_public_key(const DevicePublicKey &key) {
  return Pkey(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, key.data(), key.size()), &EVP_PKEY_free);
}

Pkey make_private_key(const DevicePrivateKey &key) {
  return Pkey(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, key.data(), key.size()), &EVP_PKEY_free);
}

Result<SymmetricKey> derive_wrapping_key(const DevicePrivateKey &private_key, const DevicePublicKey &peer_key) {
  const Pkey own_key = make_private_key(private_key);
  const Pkey peer = make_public_key(peer_key);

  if (!own_key || !peer) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const PkeyContext context(EVP_PKEY_CTX_new(own_key.get(), nullptr), &EVP_PKEY_CTX_free);

  if (!context || EVP_PKEY_derive_init(context.get()) != 1 ||
      EVP_PKEY_derive_set_peer(context.get(), peer.get()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  SecretBytes<k_device_key_size> secret{};
  std::size_t secret_size = secret.size();

  if (EVP_PKEY_derive(context.get(), secret.data(), &secret_size) != 1 || secret_size != secret.size()) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const Kdf kdf(EVP_KDF_fetch(nullptr, "HKDF", nullptr), &EVP_KDF_free);

  if (!kdf) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const KdfContext kdf_context(EVP_KDF_CTX_new(kdf.get()), &EVP_KDF_CTX_free);

  if (!kdf_context) {
    return std::unexpected(CoreError::crypto_failed);
  }

  SymmetricKey wrapping_key{};

  OSSL_PARAM params[] = {OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char *>(k_digest), 0),
                         OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, secret.data(), secret.size()),
                         OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, const_cast<char *>(k_derivation_info),
                                                           sizeof(k_derivation_info) - 1),
                         OSSL_PARAM_construct_end()};

  if (EVP_KDF_derive(kdf_context.get(), wrapping_key.data(), wrapping_key.size(), params) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return wrapping_key;
}

} // namespace

Result<DeviceKeyPair> generate_device_key() {
  const PkeyContext context(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr), &EVP_PKEY_CTX_free);

  if (!context || EVP_PKEY_keygen_init(context.get()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  EVP_PKEY *generated = nullptr;

  if (EVP_PKEY_keygen(context.get(), &generated) != 1 || generated == nullptr) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const Pkey key(generated, &EVP_PKEY_free);

  DeviceKeyPair pair{};
  std::size_t public_size = pair.public_key.size();
  std::size_t private_size = pair.private_key.size();

  if (EVP_PKEY_get_raw_public_key(key.get(), pair.public_key.data(), &public_size) != 1 ||
      EVP_PKEY_get_raw_private_key(key.get(), pair.private_key.data(), &private_size) != 1 ||
      public_size != pair.public_key.size() || private_size != pair.private_key.size()) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return pair;
}

Result<DevicePublicKey> derive_device_public_key(const DevicePrivateKey &key) {
  const Pkey private_key = make_private_key(key);

  if (!private_key) {
    return std::unexpected(CoreError::crypto_failed);
  }

  DevicePublicKey public_key{};
  std::size_t size = public_key.size();

  if (EVP_PKEY_get_raw_public_key(private_key.get(), public_key.data(), &size) != 1 || size != public_key.size()) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return public_key;
}

Result<DeviceEnvelope> seal_for_device(const SymmetricKey &key, const DevicePublicKey &device_key,
                                       Content associated_data) {
  const auto ephemeral = generate_device_key();

  if (!ephemeral.has_value()) {
    return std::unexpected(ephemeral.error());
  }

  const auto wrapping_key = derive_wrapping_key(ephemeral->private_key, device_key);

  if (!wrapping_key.has_value()) {
    return std::unexpected(wrapping_key.error());
  }

  const auto wrapped = wrap_key(key, wrapping_key.value(), associated_data);

  if (!wrapped.has_value()) {
    return std::unexpected(wrapped.error());
  }

  DeviceEnvelope envelope{};
  envelope.ephemeral_key = ephemeral->public_key;
  envelope.wrapped = wrapped.value();

  return envelope;
}

Result<SymmetricKey> open_for_device(const DeviceEnvelope &envelope, const DevicePrivateKey &device_key,
                                     Content associated_data) {
  const auto wrapping_key = derive_wrapping_key(device_key, envelope.ephemeral_key);

  if (!wrapping_key.has_value()) {
    return std::unexpected(wrapping_key.error());
  }

  return unwrap_key(envelope.wrapped, wrapping_key.value(), associated_data);
}

} // namespace dgds::core
