#include <dgds/core/watermark/mark_code.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using dgds::core::ContentIdentity;
using dgds::core::k_identity_size;
using dgds::core::k_mark_version;
using dgds::core::Mark;
using dgds::core::mark_code;
using dgds::core::PurchaseId;
using dgds::core::SymmetricKey;
using dgds::core::verify_mark_code;

constexpr PurchaseId k_purchase_id = 4242;

SymmetricKey make_key(std::uint8_t seed) {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key.data()[index] = static_cast<std::uint8_t>(seed + index);
  }

  return key;
}

ContentIdentity make_identity(std::uint8_t seed) {
  ContentIdentity identity{};

  for (std::size_t index = 0; index < k_identity_size; ++index) {
    identity[index] = static_cast<std::uint8_t>(seed + index);
  }

  return identity;
}

TEST(MarkCode, IsDeterministic) {
  const SymmetricKey key = make_key(1);
  const ContentIdentity identity = make_identity(2);

  const auto first = mark_code(key, identity, k_purchase_id, k_mark_version);
  const auto second = mark_code(key, identity, k_purchase_id, k_mark_version);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first.value(), second.value());
}

TEST(MarkCode, DependsOnKeyIdentityPurchaseAndVersion) {
  const SymmetricKey key = make_key(1);
  const SymmetricKey other_key = make_key(9);
  const ContentIdentity identity = make_identity(2);
  const ContentIdentity other_identity = make_identity(8);

  const auto code = mark_code(key, identity, k_purchase_id, k_mark_version);
  const auto other_code = mark_code(other_key, identity, k_purchase_id, k_mark_version);
  const auto identity_code = mark_code(key, other_identity, k_purchase_id, k_mark_version);
  const auto purchase_code = mark_code(key, identity, k_purchase_id + 1, k_mark_version);
  const auto version_code = mark_code(key, identity, k_purchase_id, k_mark_version + 1);

  ASSERT_TRUE(code.has_value());
  ASSERT_TRUE(other_code.has_value());
  ASSERT_TRUE(identity_code.has_value());
  ASSERT_TRUE(purchase_code.has_value());
  ASSERT_TRUE(version_code.has_value());

  EXPECT_NE(code.value(), other_code.value());
  EXPECT_NE(code.value(), identity_code.value());
  EXPECT_NE(code.value(), purchase_code.value());
  EXPECT_NE(code.value(), version_code.value());
}

TEST(MarkCode, VerifiesOwnCode) {
  const SymmetricKey key = make_key(3);
  const ContentIdentity identity = make_identity(4);

  const auto code = mark_code(key, identity, k_purchase_id, k_mark_version);
  ASSERT_TRUE(code.has_value());

  const Mark mark{.purchase_id = k_purchase_id, .code = code.value(), .version = k_mark_version};
  const auto verified = verify_mark_code(key, identity, mark);

  ASSERT_TRUE(verified.has_value());
  EXPECT_TRUE(verified.value());
}

TEST(MarkCode, RefusesCodeWithoutSecret) {
  const SymmetricKey key = make_key(3);
  const ContentIdentity identity = make_identity(4);
  const Mark forged{.purchase_id = k_purchase_id, .code = {}, .version = k_mark_version};

  const auto verified = verify_mark_code(key, identity, forged);

  ASSERT_TRUE(verified.has_value());
  EXPECT_FALSE(verified.value());
}

TEST(MarkCode, RefusesCodeOfAnotherPublication) {
  const SymmetricKey key = make_key(3);
  const ContentIdentity identity = make_identity(4);
  const ContentIdentity other_identity = make_identity(5);

  const auto code = mark_code(key, other_identity, k_purchase_id, k_mark_version);
  ASSERT_TRUE(code.has_value());

  const Mark mark{.purchase_id = k_purchase_id, .code = code.value(), .version = k_mark_version};
  const auto verified = verify_mark_code(key, identity, mark);

  ASSERT_TRUE(verified.has_value());
  EXPECT_FALSE(verified.value());
}

TEST(MarkCode, RefusesDamagedCode) {
  const SymmetricKey key = make_key(6);
  const ContentIdentity identity = make_identity(7);

  const auto code = mark_code(key, identity, k_purchase_id, k_mark_version);
  ASSERT_TRUE(code.has_value());

  Mark mark{.purchase_id = k_purchase_id, .code = code.value(), .version = k_mark_version};
  mark.code[3] = static_cast<std::uint8_t>(mark.code[3] ^ 1U);

  const auto verified = verify_mark_code(key, identity, mark);

  ASSERT_TRUE(verified.has_value());
  EXPECT_FALSE(verified.value());
}

} // namespace
