#include <dgds/core/crypto/aead.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>

namespace dgds::core {
namespace {

constexpr std::size_t k_chunk_size = 1U << 20;
constexpr std::size_t k_algorithm_size = 1;

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

const unsigned char *as_bytes(Content content) { return reinterpret_cast<const unsigned char *>(content.data()); }

bool fits_into_int(std::size_t size) { return size <= static_cast<std::size_t>(std::numeric_limits<int>::max()); }

} // namespace

Result<SealedContent> encrypt(Content plaintext, const SymmetricKey &key, Content associated_data) {
  if (!fits_into_int(associated_data.size())) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const CipherContext context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);

  if (!context) {
    return std::unexpected(CoreError::crypto_failed);
  }

  SealedContent sealed{};
  sealed.algorithm = k_aead_algorithm;

  if (RAND_bytes(sealed.nonce.data(), static_cast<int>(sealed.nonce.size())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  if (EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
      EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(sealed.nonce.size()), nullptr) != 1 ||
      EVP_EncryptInit_ex(context.get(), nullptr, nullptr, key.data(), sealed.nonce.data()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const auto algorithm = static_cast<unsigned char>(sealed.algorithm);
  int written = 0;

  if (EVP_EncryptUpdate(context.get(), nullptr, &written, &algorithm, static_cast<int>(k_algorithm_size)) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  if (!associated_data.empty() && EVP_EncryptUpdate(context.get(), nullptr, &written, as_bytes(associated_data),
                                                    static_cast<int>(associated_data.size())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  sealed.ciphertext.resize(plaintext.size());

  std::size_t offset = 0;

  while (offset < plaintext.size()) {
    const std::size_t chunk = std::min(plaintext.size() - offset, k_chunk_size);

    if (EVP_EncryptUpdate(context.get(), sealed.ciphertext.data() + offset, &written, as_bytes(plaintext) + offset,
                          static_cast<int>(chunk)) != 1) {
      return std::unexpected(CoreError::crypto_failed);
    }

    offset += chunk;
  }

  int final_length = 0;

  if (EVP_EncryptFinal_ex(context.get(), sealed.ciphertext.data() + offset, &final_length) != 1 ||
      EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG, static_cast<int>(sealed.tag.size()),
                          sealed.tag.data()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  sealed.ciphertext.resize(offset + static_cast<std::size_t>(final_length));

  return sealed;
}

Result<ContentBuffer> decrypt(const SealedContent &sealed, const SymmetricKey &key, Content associated_data) {
  if (sealed.algorithm != k_aead_algorithm) {
    return std::unexpected(CoreError::algorithm_unsupported);
  }

  if (!fits_into_int(associated_data.size())) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const CipherContext context(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);

  if (!context) {
    return std::unexpected(CoreError::crypto_failed);
  }

  if (EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
      EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(sealed.nonce.size()), nullptr) != 1 ||
      EVP_DecryptInit_ex(context.get(), nullptr, nullptr, key.data(), sealed.nonce.data()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const auto algorithm = static_cast<unsigned char>(sealed.algorithm);
  int written = 0;

  if (EVP_DecryptUpdate(context.get(), nullptr, &written, &algorithm, static_cast<int>(k_algorithm_size)) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  if (!associated_data.empty() && EVP_DecryptUpdate(context.get(), nullptr, &written, as_bytes(associated_data),
                                                    static_cast<int>(associated_data.size())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  ContentBuffer plaintext;
  plaintext.resize(sealed.ciphertext.size());

  std::size_t offset = 0;

  while (offset < sealed.ciphertext.size()) {
    const std::size_t chunk = std::min(sealed.ciphertext.size() - offset, k_chunk_size);

    if (EVP_DecryptUpdate(context.get(), reinterpret_cast<unsigned char *>(plaintext.data()) + offset, &written,
                          sealed.ciphertext.data() + offset, static_cast<int>(chunk)) != 1) {
      return std::unexpected(CoreError::crypto_failed);
    }

    offset += chunk;
  }

  const Tag tag = sealed.tag;

  if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()),
                          const_cast<std::uint8_t *>(tag.data())) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  int final_length = 0;

  if (EVP_DecryptFinal_ex(context.get(), reinterpret_cast<unsigned char *>(plaintext.data()) + offset, &final_length) !=
      1) {
    return std::unexpected(CoreError::authentication_failed);
  }

  plaintext.resize(offset + static_cast<std::size_t>(final_length));

  return plaintext;
}

} // namespace dgds::core
