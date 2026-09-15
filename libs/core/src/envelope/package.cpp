#include <dgds/core/envelope/package.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/crypto/secure_buffer.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/signature/author_signature.h>

#include <cstddef>
#include <expected>

namespace dgds::core {
namespace {

Content as_content(const ContentIdentity &identity) {
  return Content(reinterpret_cast<const char *>(identity.data()), identity.size());
}

} // namespace

Result<SecureBuffer> open_package(const Package &package, const SymmetricKey &purchase_key) {
  if (package.version != k_package_version) {
    return std::unexpected(CoreError::package_version_unsupported);
  }

  const Content bound_identity = as_content(package.identity);

  const auto blob_key = unwrap_key(package.wrapped_blob_key, purchase_key, bound_identity);

  if (!blob_key.has_value()) {
    return std::unexpected(blob_key.error());
  }

  auto plaintext = decrypt(package.content, blob_key.value(), bound_identity);

  if (!plaintext.has_value()) {
    return std::unexpected(plaintext.error());
  }

  const auto identity = content_identity(plaintext->view());

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  if (identity.value() != package.identity) {
    return std::unexpected(CoreError::content_mismatch);
  }

  const auto verified = verify_author(identity.value(), package.author_name, package.signature, package.author_key);

  if (!verified.has_value()) {
    return std::unexpected(verified.error());
  }

  if (!verified.value()) {
    return std::unexpected(CoreError::signature_invalid);
  }

  return plaintext;
}

} // namespace dgds::core
