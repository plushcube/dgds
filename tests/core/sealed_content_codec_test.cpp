#include <dgds/core/crypto/aead.h>
#include <dgds/core/crypto/sealed_content_codec.h>
#include <dgds/core/envelope/keys.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace {

using dgds::core::Content;
using dgds::core::CoreError;
using dgds::core::decode_sealed_content;
using dgds::core::encode_sealed_content;
using dgds::core::generate_key;
using dgds::core::Result;
using dgds::core::SealedContent;
using dgds::core::SymmetricKey;

constexpr std::string_view k_plaintext = "секретный текст";
constexpr std::string_view k_associated_data = "ad";

Result<SealedContent> make_sealed(const SymmetricKey &key) {
  return dgds::core::encrypt(k_plaintext, key, k_associated_data);
}

TEST(SealedContentCodec, RoundTripKeepsContentOpenable) {
  const auto key = generate_key();
  ASSERT_TRUE(key.has_value());

  const auto sealed = make_sealed(key.value());
  ASSERT_TRUE(sealed.has_value());

  const auto encoded = encode_sealed_content(sealed.value());
  ASSERT_TRUE(encoded.has_value());

  const auto decoded = decode_sealed_content(encoded.value());
  ASSERT_TRUE(decoded.has_value());

  EXPECT_EQ(decoded->algorithm, sealed->algorithm);
  EXPECT_EQ(decoded->nonce, sealed->nonce);
  EXPECT_EQ(decoded->ciphertext, sealed->ciphertext);
  EXPECT_EQ(decoded->tag, sealed->tag);

  const auto opened = dgds::core::decrypt(decoded.value(), key.value(), k_associated_data);

  ASSERT_TRUE(opened.has_value());
  EXPECT_EQ(opened->view(), k_plaintext);
}

TEST(SealedContentCodec, RejectsTruncatedData) {
  const auto key = generate_key();
  ASSERT_TRUE(key.has_value());

  const auto sealed = make_sealed(key.value());
  ASSERT_TRUE(sealed.has_value());

  const auto encoded = encode_sealed_content(sealed.value());
  ASSERT_TRUE(encoded.has_value());
  ASSERT_GT(encoded->size(), 1U);

  for (std::size_t length = 0; length < encoded->size(); ++length) {
    const auto decoded = decode_sealed_content(Content(encoded->data(), length));

    ASSERT_FALSE(decoded.has_value()) << length;
    EXPECT_EQ(decoded.error(), CoreError::sealed_content_malformed) << length;
  }
}

TEST(SealedContentCodec, RejectsTrailingBytes) {
  const auto key = generate_key();
  ASSERT_TRUE(key.has_value());

  const auto sealed = make_sealed(key.value());
  ASSERT_TRUE(sealed.has_value());

  auto encoded = encode_sealed_content(sealed.value());
  ASSERT_TRUE(encoded.has_value());

  encoded->push_back('\0');

  const auto decoded = decode_sealed_content(encoded.value());

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), CoreError::sealed_content_malformed);
}

TEST(SealedContentCodec, RejectsUnclaimedCiphertextLength) {
  const auto key = generate_key();
  ASSERT_TRUE(key.has_value());

  const auto sealed = make_sealed(key.value());
  ASSERT_TRUE(sealed.has_value());

  auto encoded = encode_sealed_content(sealed.value());
  ASSERT_TRUE(encoded.has_value());

  constexpr std::size_t k_length_offset = 1 + dgds::core::k_nonce_size;
  constexpr std::size_t k_length_size = sizeof(std::uint32_t);

  ASSERT_GT(encoded->size(), k_length_offset + k_length_size);

  for (std::size_t index = 0; index < k_length_size; ++index) {
    (*encoded)[k_length_offset + index] = static_cast<char>(0xFF);
  }

  const auto decoded = decode_sealed_content(encoded.value());

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), CoreError::sealed_content_malformed);
}

} // namespace
