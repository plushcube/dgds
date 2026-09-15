#include <dgds/core/envelope/keys.h>

#include <dgds/core/crypto/aead.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using dgds::core::CoreError;
using dgds::core::generate_key;
using dgds::core::SymmetricKey;
using dgds::core::unwrap_key;
using dgds::core::wrap_key;

constexpr const char k_first_purchase[] = "покупка 1";
constexpr const char k_second_purchase[] = "покупка 2";

SymmetricKey make_key(std::uint8_t seed) {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key.data()[index] = static_cast<std::uint8_t>(seed + index);
  }

  return key;
}

TEST(Keys, GeneratesDifferentKeys) {
  const auto first = generate_key();
  const auto second = generate_key();

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_FALSE(first->equals(second.value()));
}

TEST(Keys, UnwrapsWrappedKey) {
  const SymmetricKey key = make_key(1);
  const SymmetricKey purchase_key = make_key(40);

  const auto wrapped = wrap_key(key, purchase_key, k_first_purchase);
  ASSERT_TRUE(wrapped.has_value());

  const auto unwrapped = unwrap_key(wrapped.value(), purchase_key, k_first_purchase);

  ASSERT_TRUE(unwrapped.has_value());
  EXPECT_TRUE(unwrapped->equals(key));
}

TEST(Keys, GivesDifferentWrapsForSameKey) {
  const SymmetricKey key = make_key(2);

  const auto first = wrap_key(key, make_key(40), k_first_purchase);
  const auto second = wrap_key(key, make_key(41), k_second_purchase);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_NE(first->ciphertext, second->ciphertext);

  const auto repeated = wrap_key(key, make_key(40), k_first_purchase);

  ASSERT_TRUE(repeated.has_value());
  EXPECT_NE(first->nonce, repeated->nonce);
}

TEST(Keys, RejectsForeignWrappingKey) {
  const SymmetricKey key = make_key(3);

  const auto wrapped = wrap_key(key, make_key(50), k_first_purchase);
  ASSERT_TRUE(wrapped.has_value());

  const auto unwrapped = unwrap_key(wrapped.value(), make_key(51), k_first_purchase);

  ASSERT_FALSE(unwrapped.has_value());
  EXPECT_EQ(unwrapped.error(), CoreError::authentication_failed);
}

TEST(Keys, RejectsWrapOfAnotherPurchase) {
  const SymmetricKey key = make_key(4);
  const SymmetricKey purchase_key = make_key(60);

  const auto wrapped = wrap_key(key, purchase_key, k_first_purchase);
  ASSERT_TRUE(wrapped.has_value());

  const auto unwrapped = unwrap_key(wrapped.value(), purchase_key, k_second_purchase);

  ASSERT_FALSE(unwrapped.has_value());
  EXPECT_EQ(unwrapped.error(), CoreError::authentication_failed);
}

TEST(Keys, RejectsWrappedValueOfWrongSize) {
  const SymmetricKey purchase_key = make_key(70);

  const auto wrapped = wrap_key(make_key(5), purchase_key, k_first_purchase);
  ASSERT_TRUE(wrapped.has_value());

  const auto short_wrap = dgds::core::encrypt("короткое значение", purchase_key, k_first_purchase);
  ASSERT_TRUE(short_wrap.has_value());

  const auto unwrapped = unwrap_key(short_wrap.value(), purchase_key, k_first_purchase);

  ASSERT_FALSE(unwrapped.has_value());
  EXPECT_EQ(unwrapped.error(), CoreError::key_size_mismatch);
}

} // namespace
