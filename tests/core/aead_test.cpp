#include <dgds/core/crypto/aead.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

using dgds::core::AeadAlgorithm;
using dgds::core::CoreError;
using dgds::core::decrypt;
using dgds::core::encrypt;
using dgds::core::SymmetricKey;

SymmetricKey make_key(std::uint8_t seed) {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key[index] = static_cast<std::uint8_t>(seed + index);
  }

  return key;
}

constexpr const char k_associated_data[] = "идентификатор покупки";

TEST(Aead, RoundTripKeepsContent) {
  const SymmetricKey key = make_key(1);
  const std::string text = "контент публикации";

  const auto sealed = encrypt(text, key, k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto opened = decrypt(sealed.value(), key, k_associated_data);

  ASSERT_TRUE(opened.has_value());
  EXPECT_EQ(opened->view(), text);
}

TEST(Aead, RoundTripKeepsEmptyContent) {
  const SymmetricKey key = make_key(2);

  const auto sealed = encrypt("", key, k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto opened = decrypt(sealed.value(), key, k_associated_data);

  ASSERT_TRUE(opened.has_value());
  EXPECT_TRUE(opened->empty());
}

TEST(Aead, RejectsWrongKey) {
  const std::string text = "контент публикации";

  const auto sealed = encrypt(text, make_key(3), k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto opened = decrypt(sealed.value(), make_key(4), k_associated_data);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST(Aead, RejectsTamperedCiphertext) {
  const SymmetricKey key = make_key(5);

  const auto sealed = encrypt("контент публикации", key, k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  auto tampered = sealed.value();
  tampered.ciphertext.front() = static_cast<std::uint8_t>(tampered.ciphertext.front() ^ 1U);

  const auto opened = decrypt(tampered, key, k_associated_data);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST(Aead, RejectsTamperedAssociatedData) {
  const SymmetricKey key = make_key(6);

  const auto sealed = encrypt("контент публикации", key, k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto opened = decrypt(sealed.value(), key, "другой идентификатор покупки");

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST(Aead, RejectsUnsupportedAlgorithm) {
  const SymmetricKey key = make_key(7);

  auto sealed = encrypt("контент публикации", key, k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  sealed->algorithm = static_cast<AeadAlgorithm>(0x7F);

  const auto opened = decrypt(sealed.value(), key, k_associated_data);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::algorithm_unsupported);
}

TEST(Aead, UsesFreshNonce) {
  const SymmetricKey key = make_key(8);
  const std::string text = "контент публикации";

  const auto first = encrypt(text, key, k_associated_data);
  const auto second = encrypt(text, key, k_associated_data);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_NE(first->nonce, second->nonce);
  EXPECT_NE(first->ciphertext, second->ciphertext);
}

} // namespace
