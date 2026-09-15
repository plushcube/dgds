#include <dgds/core/envelope/package.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/signature/author_signature.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <expected>

namespace {

using dgds::core::AuthorKeyPair;
using dgds::core::Content;
using dgds::core::ContentBuffer;
using dgds::core::ContentIdentity;
using dgds::core::CoreError;
using dgds::core::encrypt;
using dgds::core::generate_author_key;
using dgds::core::generate_key;
using dgds::core::k_package_version;
using dgds::core::open_package;
using dgds::core::Package;
using dgds::core::sign_author;
using dgds::core::SymmetricKey;
using dgds::core::wrap_key;

constexpr const char k_author[] = "автор";
constexpr const char k_plaintext[] = "контент публикации";

Content as_content(const ContentIdentity &identity) {
  return Content(reinterpret_cast<const char *>(identity.data()), identity.size());
}

Package assemble(Content plaintext, const ContentIdentity &claimed, const SymmetricKey &file_key,
                 const SymmetricKey &purchase_key, const AuthorKeyPair &author) {
  const Content bound_identity = as_content(claimed);

  Package package{};
  package.version = k_package_version;
  package.content = encrypt(plaintext, file_key, bound_identity).value();
  package.wrapped_blob_key = wrap_key(file_key, purchase_key, bound_identity).value();
  package.identity = claimed;
  package.author_key = author.public_key;
  package.author_name = ContentBuffer(k_author);
  package.signature = sign_author(claimed, k_author, author.private_key).value();

  return package;
}

TEST(Package, OpensValidPackage) {
  const auto file_key = generate_key();
  const auto purchase_key = generate_key();
  const auto author = generate_author_key();
  const auto identity = dgds::core::content_identity(k_plaintext);

  ASSERT_TRUE(file_key.has_value());
  ASSERT_TRUE(purchase_key.has_value());
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(identity.has_value());

  const Package package =
      assemble(k_plaintext, identity.value(), file_key.value(), purchase_key.value(), author.value());

  const auto opened = open_package(package, purchase_key.value());

  ASSERT_TRUE(opened.has_value());
  EXPECT_EQ(opened->view(), k_plaintext);
}

TEST(Package, RejectsUnsupportedVersion) {
  const auto file_key = generate_key();
  const auto purchase_key = generate_key();
  const auto author = generate_author_key();
  const auto identity = dgds::core::content_identity(k_plaintext);

  ASSERT_TRUE(file_key.has_value());
  ASSERT_TRUE(purchase_key.has_value());
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(identity.has_value());

  Package package = assemble(k_plaintext, identity.value(), file_key.value(), purchase_key.value(), author.value());
  package.version = k_package_version + 1;

  const auto opened = open_package(package, purchase_key.value());

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::package_version_unsupported);
}

TEST(Package, RejectsTamperedCiphertext) {
  const auto file_key = generate_key();
  const auto purchase_key = generate_key();
  const auto author = generate_author_key();
  const auto identity = dgds::core::content_identity(k_plaintext);

  ASSERT_TRUE(file_key.has_value());
  ASSERT_TRUE(purchase_key.has_value());
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(identity.has_value());

  Package package = assemble(k_plaintext, identity.value(), file_key.value(), purchase_key.value(), author.value());
  package.content.ciphertext.front() = static_cast<std::uint8_t>(package.content.ciphertext.front() ^ 1U);

  const auto opened = open_package(package, purchase_key.value());

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST(Package, RejectsMismatchedIdentity) {
  const auto file_key = generate_key();
  const auto purchase_key = generate_key();
  const auto author = generate_author_key();
  const auto claimed = dgds::core::content_identity(k_plaintext);

  ASSERT_TRUE(file_key.has_value());
  ASSERT_TRUE(purchase_key.has_value());
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(claimed.has_value());

  const Package package =
      assemble("подменённый текст", claimed.value(), file_key.value(), purchase_key.value(), author.value());

  const auto opened = open_package(package, purchase_key.value());

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::content_mismatch);
}

TEST(Package, RejectsForeignSignature) {
  const auto file_key = generate_key();
  const auto purchase_key = generate_key();
  const auto author = generate_author_key();
  const auto foreign = generate_author_key();
  const auto identity = dgds::core::content_identity(k_plaintext);

  ASSERT_TRUE(file_key.has_value());
  ASSERT_TRUE(purchase_key.has_value());
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(foreign.has_value());
  ASSERT_TRUE(identity.has_value());

  Package package = assemble(k_plaintext, identity.value(), file_key.value(), purchase_key.value(), author.value());
  package.author_key = foreign->public_key;

  const auto opened = open_package(package, purchase_key.value());

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::signature_invalid);
}

TEST(Package, RejectsForeignPurchaseKey) {
  const auto file_key = generate_key();
  const auto purchase_key = generate_key();
  const auto foreign = generate_key();
  const auto author = generate_author_key();
  const auto identity = dgds::core::content_identity(k_plaintext);

  ASSERT_TRUE(file_key.has_value());
  ASSERT_TRUE(purchase_key.has_value());
  ASSERT_TRUE(foreign.has_value());
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(identity.has_value());

  const Package package =
      assemble(k_plaintext, identity.value(), file_key.value(), purchase_key.value(), author.value());

  const auto opened = open_package(package, foreign.value());

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

} // namespace
