#include <dgds/core/signature/author_signature.h>

#include <dgds/core/models/author.h>

#include <openssl/evp.h>

#include <cstddef>
#include <expected>
#include <memory>

namespace dgds::core {
namespace {

using Pkey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PkeyContext = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

const unsigned char *as_bytes(Content content) { return reinterpret_cast<const unsigned char *>(content.data()); }

ContentBuffer signed_message(const ContentIdentity &identity, Content author_name) {
  ContentBuffer message;
  message.reserve(identity.size() + author_name.size());
  message.append(reinterpret_cast<const char *>(identity.data()), identity.size());
  message.append(author_name);

  return message;
}

Pkey make_public_key(const AuthorPublicKey &key) {
  return Pkey(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, key.data(), key.size()), &EVP_PKEY_free);
}

Pkey make_private_key(const AuthorPrivateKey &key) {
  return Pkey(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, key.data(), key.size()), &EVP_PKEY_free);
}

} // namespace

Result<AuthorKeyPair> generate_author_key() {
  const PkeyContext context(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr), &EVP_PKEY_CTX_free);

  if (!context || EVP_PKEY_keygen_init(context.get()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  EVP_PKEY *generated = nullptr;

  if (EVP_PKEY_keygen(context.get(), &generated) != 1 || generated == nullptr) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const Pkey key(generated, &EVP_PKEY_free);

  AuthorKeyPair pair{};
  std::size_t public_size = pair.public_key.size();
  std::size_t private_size = pair.private_key.size();

  if (EVP_PKEY_get_raw_public_key(key.get(), pair.public_key.data(), &public_size) != 1 ||
      EVP_PKEY_get_raw_private_key(key.get(), pair.private_key.data(), &private_size) != 1 ||
      public_size != pair.public_key.size() || private_size != pair.private_key.size()) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return pair;
}

Result<Signature> sign_author(const ContentIdentity &identity, Content author_name, const AuthorPrivateKey &key) {
  const Pkey private_key = make_private_key(key);

  if (!private_key) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const ContentBuffer message = signed_message(identity, author_name);
  const DigestContext context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);

  if (!context || EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, private_key.get()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  Signature signature{};
  std::size_t size = signature.size();

  if (EVP_DigestSign(context.get(), signature.data(), &size, as_bytes(message), message.size()) != 1 ||
      size != signature.size()) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return signature;
}

Result<bool> verify_author(const ContentIdentity &identity, Content author_name, const Signature &signature,
                           const AuthorPublicKey &key) {
  const Pkey public_key = make_public_key(key);

  if (!public_key) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const ContentBuffer message = signed_message(identity, author_name);
  const DigestContext context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);

  if (!context || EVP_DigestVerifyInit(context.get(), nullptr, nullptr, nullptr, public_key.get()) != 1) {
    return std::unexpected(CoreError::crypto_failed);
  }

  const int status =
      EVP_DigestVerify(context.get(), signature.data(), signature.size(), as_bytes(message), message.size());

  if (status < 0) {
    return std::unexpected(CoreError::crypto_failed);
  }

  return status == 1;
}

} // namespace dgds::core
