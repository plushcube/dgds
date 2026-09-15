#include <dgds/core/signature/author_signature.h>

#include <dgds/core/identity/content_identity.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using dgds::core::content_identity;
using dgds::core::ContentIdentity;
using dgds::core::generate_author_key;
using dgds::core::sign_author;
using dgds::core::verify_author;

constexpr const char k_author[] = "автор";
constexpr const char k_other_author[] = "другой автор";

ContentIdentity make_identity(std::uint8_t seed) {
  ContentIdentity identity{};

  for (std::size_t index = 0; index < identity.size(); ++index) {
    identity[index] = static_cast<std::uint8_t>(seed + index);
  }

  return identity;
}

TEST(AuthorSignature, GeneratesDifferentKeys) {
  const auto first = generate_author_key();
  const auto second = generate_author_key();

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_NE(first->public_key, second->public_key);
  EXPECT_NE(first->private_key, second->private_key);
}

TEST(AuthorSignature, VerifiesSignatureOfOwnKey) {
  const auto key = generate_author_key();
  ASSERT_TRUE(key.has_value());

  const ContentIdentity identity = make_identity(1);

  const auto signature = sign_author(identity, k_author, key->private_key);
  ASSERT_TRUE(signature.has_value());

  const auto verified = verify_author(identity, k_author, signature.value(), key->public_key);

  ASSERT_TRUE(verified.has_value());
  EXPECT_TRUE(verified.value());
}

TEST(AuthorSignature, RejectsChangedContent) {
  const auto key = generate_author_key();
  ASSERT_TRUE(key.has_value());

  const auto signature = sign_author(make_identity(2), k_author, key->private_key);
  ASSERT_TRUE(signature.has_value());

  const auto verified = verify_author(make_identity(3), k_author, signature.value(), key->public_key);

  ASSERT_TRUE(verified.has_value());
  EXPECT_FALSE(verified.value());
}

TEST(AuthorSignature, RejectsChangedAuthorName) {
  const auto key = generate_author_key();
  ASSERT_TRUE(key.has_value());

  const ContentIdentity identity = make_identity(4);

  const auto signature = sign_author(identity, k_author, key->private_key);
  ASSERT_TRUE(signature.has_value());

  const auto verified = verify_author(identity, k_other_author, signature.value(), key->public_key);

  ASSERT_TRUE(verified.has_value());
  EXPECT_FALSE(verified.value());
}

TEST(AuthorSignature, RejectsForeignKey) {
  const auto key = generate_author_key();
  const auto foreign = generate_author_key();
  ASSERT_TRUE(key.has_value());
  ASSERT_TRUE(foreign.has_value());

  const ContentIdentity identity = make_identity(5);

  const auto signature = sign_author(identity, k_author, key->private_key);
  ASSERT_TRUE(signature.has_value());

  const auto verified = verify_author(identity, k_author, signature.value(), foreign->public_key);

  ASSERT_TRUE(verified.has_value());
  EXPECT_FALSE(verified.value());
}

TEST(AuthorSignature, RejectsTamperedSignature) {
  const auto key = generate_author_key();
  ASSERT_TRUE(key.has_value());

  const ContentIdentity identity = make_identity(6);

  const auto signature = sign_author(identity, k_author, key->private_key);
  ASSERT_TRUE(signature.has_value());

  auto tampered = signature.value();
  tampered.front() = static_cast<std::uint8_t>(tampered.front() ^ 1U);

  const auto verified = verify_author(identity, k_author, tampered, key->public_key);

  ASSERT_TRUE(verified.has_value());
  EXPECT_FALSE(verified.value());
}

TEST(AuthorSignature, VerifiesContentThatDiffersOnlyByMarks) {
  const auto key = generate_author_key();
  ASSERT_TRUE(key.has_value());

  const auto plain = content_identity("текст автора");
  const auto marked = content_identity("тек\u200Bст автора");
  ASSERT_TRUE(plain.has_value());
  ASSERT_TRUE(marked.has_value());

  const auto signature = sign_author(plain.value(), k_author, key->private_key);
  ASSERT_TRUE(signature.has_value());

  const auto verified = verify_author(marked.value(), k_author, signature.value(), key->public_key);

  ASSERT_TRUE(verified.has_value());
  EXPECT_TRUE(verified.value());
}

} // namespace
