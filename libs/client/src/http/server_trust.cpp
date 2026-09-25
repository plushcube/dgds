#include <dgds/client/http/server_trust.h>

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string>

namespace dgds::client {
namespace {

template <typename Type, void (*Release)(Type *)> class Handle {
public:
  Handle(Type *value = nullptr) : p_value(value) {}
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  Handle(Handle &&) = delete;
  Handle &operator=(Handle &&) = delete;
  ~Handle() {
    if (p_value != nullptr) {
      Release(p_value);
    }
  }

  [[nodiscard]] Type *get() const { return p_value; }

private:
  Type *p_value;
};

void close_bio(BIO *bio) { BIO_free(bio); }

using BioHandle = Handle<BIO, close_bio>;
using CertificateHandle = Handle<X509, X509_free>;
void release_encoded(unsigned char *bytes) { OPENSSL_free(bytes); }

using EncodedBytes = std::unique_ptr<unsigned char, void (*)(unsigned char *)>;

core::Result<ServerPin> public_key_digest(std::span<const unsigned char> certificate_der) {
  const unsigned char *cursor = certificate_der.data();
  const CertificateHandle certificate{d2i_X509(nullptr, &cursor, static_cast<long>(certificate_der.size()))};

  if (certificate.get() == nullptr) {
    return std::unexpected(core::CoreError::key_malformed);
  }

  X509_PUBKEY *public_key = X509_get_X509_PUBKEY(certificate.get());

  if (public_key == nullptr) {
    return std::unexpected(core::CoreError::key_malformed);
  }

  unsigned char *encoded = nullptr;
  const int length = i2d_X509_PUBKEY(public_key, &encoded);
  const EncodedBytes owned{encoded, release_encoded};

  if (length <= 0) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  ServerPinBytes digest{};
  unsigned int written = 0;

  if (EVP_Digest(owned.get(), static_cast<std::size_t>(length), digest.data(), &written, EVP_sha256(), nullptr) != 1 ||
      written != k_server_pin_size) {
    return std::unexpected(core::CoreError::digest_failed);
  }

  return ServerPin{digest};
}

} // namespace

core::Result<ServerTrust> load_server_trust(const std::filesystem::path &certificate) {
  const BioHandle input{BIO_new_file(certificate.c_str(), "r")};

  if (input.get() == nullptr) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const CertificateHandle parsed{PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr)};

  if (parsed.get() == nullptr) {
    return std::unexpected(core::CoreError::key_malformed);
  }

  unsigned char *encoded = nullptr;
  const int length = i2d_X509(parsed.get(), &encoded);
  const EncodedBytes owned{encoded, release_encoded};

  if (length <= 0) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  const auto pin = public_key_digest({owned.get(), static_cast<std::size_t>(length)});

  if (!pin.has_value()) {
    return std::unexpected(pin.error());
  }

  return ServerTrust{.pin = pin.value(), .certificate = certificate};
}

bool pinned_matches(const ServerPin &pin, std::span<const unsigned char> certificate_der) {
  const auto digest = public_key_digest(certificate_der);

  return digest.has_value() && digest.value() == pin;
}

} // namespace dgds::client
