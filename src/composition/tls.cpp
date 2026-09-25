#include "tls.h"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

namespace dgds::app {
namespace {

constexpr const char *k_certificate_name = "server.crt";
constexpr const char *k_key_name = "server.key";
constexpr const char *k_common_name = "dgds-server";
constexpr const char *k_alternative_names = "DNS:localhost,IP:127.0.0.1";
constexpr long k_valid_seconds = 10L * 365 * 24 * 60 * 60;
constexpr int k_serial = 1;
constexpr std::size_t k_fingerprint_size = 32;
constexpr unsigned char k_low_nibble_mask = 0x0F;

template <typename Type, void (*Release)(Type *)> class Handle {
public:
  Handle(Type *value = nullptr) : p_value(value) {}
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  Handle(Handle &&other) noexcept : p_value(other.p_value) { other.p_value = nullptr; }
  Handle &operator=(Handle &&other) noexcept {
    if (this != &other) {
      if (p_value != nullptr) {
        Release(p_value);
      }

      p_value = other.p_value;
      other.p_value = nullptr;
    }

    return *this;
  }
  ~Handle() {
    if (p_value != nullptr) {
      Release(p_value);
    }
  }

  [[nodiscard]] Type *get() const { return p_value; }

private:
  Type *p_value;
};

using KeyHandle = Handle<EVP_PKEY, EVP_PKEY_free>;
using CertificateHandle = Handle<X509, X509_free>;
void close_bio(BIO *bio) { BIO_free(bio); }

using BioHandle = Handle<BIO, close_bio>;
using ExtensionHandle = Handle<X509_EXTENSION, X509_EXTENSION_free>;
using DigestHandle = Handle<EVP_MD_CTX, EVP_MD_CTX_free>;

KeyHandle make_key() { return KeyHandle(EVP_EC_gen("P-256")); }

CertificateHandle make_certificate(EVP_PKEY *key) {
  CertificateHandle certificate{X509_new()};

  if (certificate.get() == nullptr) {
    return {};
  }

  X509_set_version(certificate.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(certificate.get()), k_serial);
  X509_gmtime_adj(X509_getm_notBefore(certificate.get()), 0);
  X509_gmtime_adj(X509_getm_notAfter(certificate.get()), k_valid_seconds);
  X509_set_pubkey(certificate.get(), key);

  X509_NAME *name = X509_get_subject_name(certificate.get());
  const unsigned char *common_name = reinterpret_cast<const unsigned char *>(k_common_name);

  if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, common_name, -1, -1, 0) != 1) {
    return {};
  }

  X509_set_issuer_name(certificate.get(), name);

  X509V3_CTX context;
  X509V3_set_ctx_nodb(&context);
  X509V3_set_ctx(&context, certificate.get(), certificate.get(), nullptr, nullptr, 0);

  const ExtensionHandle alternative_names{
      X509V3_EXT_conf_nid(nullptr, &context, NID_subject_alt_name, k_alternative_names)};

  if (alternative_names.get() == nullptr) {
    return {};
  }

  if (X509_add_ext(certificate.get(), alternative_names.get(), -1) != 1) {
    return {};
  }

  if (X509_sign(certificate.get(), key, EVP_sha256()) == 0) {
    return {};
  }

  return certificate;
}

core::Result<void> write_files(const TlsFiles &files, X509 *certificate, EVP_PKEY *key) {
  std::error_code status;
  std::filesystem::create_directories(files.certificate.parent_path(), status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const BioHandle certificate_bio{BIO_new_file(files.certificate.c_str(), "w")};

  if (certificate_bio.get() == nullptr || PEM_write_bio_X509(certificate_bio.get(), certificate) != 1) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const BioHandle key_bio{BIO_new_file(files.key.c_str(), "w")};

  if (key_bio.get() == nullptr ||
      PEM_write_bio_PrivateKey(key_bio.get(), key, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  std::filesystem::permissions(files.key, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::replace, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return {};
}

} // namespace

core::Result<TlsFiles> load_or_create_certificate(const std::filesystem::path &directory) {
  const TlsFiles files{.certificate = directory / k_certificate_name, .key = directory / k_key_name};

  std::error_code status;

  if (std::filesystem::exists(files.certificate, status) && std::filesystem::exists(files.key, status)) {
    return files;
  }

  const KeyHandle key = make_key();

  if (key.get() == nullptr) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  const CertificateHandle certificate = make_certificate(key.get());

  if (certificate.get() == nullptr) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  const auto written = write_files(files, certificate.get(), key.get());

  if (!written.has_value()) {
    return std::unexpected(written.error());
  }

  return files;
}

core::Result<std::string> certificate_fingerprint(const std::filesystem::path &certificate) {
  const BioHandle input{BIO_new_file(certificate.c_str(), "r")};

  if (input.get() == nullptr) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const CertificateHandle parsed{PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr)};

  if (parsed.get() == nullptr) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  std::array<unsigned char, k_fingerprint_size> digest{};
  unsigned int length = 0;

  if (X509_digest(parsed.get(), EVP_sha256(), digest.data(), &length) != 1 || length != k_fingerprint_size) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  std::string text;
  constexpr char k_digits[] = "0123456789abcdef";

  for (const unsigned char byte : digest) {
    if (!text.empty()) {
      text.push_back(':');
    }

    text.push_back(k_digits[byte >> 4]);
    text.push_back(k_digits[byte & k_low_nibble_mask]);
  }

  return text;
}

} // namespace dgds::app
