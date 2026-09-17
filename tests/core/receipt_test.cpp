#include <dgds/core/envelope/receipt.h>

#include <dgds/core/envelope/device_wrap.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using dgds::core::CoreError;
using dgds::core::generate_device_key;
using dgds::core::k_receipt_version;
using dgds::core::open_receipt_key;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::SymmetricKey;
using dgds::core::UserId;
using dgds::core::wrap_receipt_key;

SymmetricKey make_key(std::uint8_t seed) {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key.data()[index] = static_cast<std::uint8_t>(seed + index);
  }

  return key;
}

UserId make_user_id(std::uint8_t seed) {
  UserId user_id{};

  for (std::size_t index = 0; index < user_id.size(); ++index) {
    user_id[index] = static_cast<std::uint8_t>(seed * 3 + index);
  }

  return user_id;
}

ReceiptHeader make_header(std::uint8_t seed) {
  return ReceiptHeader{.version = k_receipt_version,
                       .purchase_id = seed,
                       .user_id = make_user_id(seed),
                       .purchased_at = 1700000000 + seed,
                       .issued_at = 1700000100 + seed};
}

TEST(Receipt, OpensReceiptOfOwnDevice) {
  const SymmetricKey purchase_key = make_key(1);
  const auto device = generate_device_key();
  const ReceiptHeader header = make_header(1);

  ASSERT_TRUE(device.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  const auto opened = open_receipt_key(receipt, device->private_key);

  ASSERT_TRUE(opened.has_value());
  EXPECT_TRUE(opened->equals(purchase_key));
}

TEST(Receipt, RejectsUnsupportedVersion) {
  const SymmetricKey purchase_key = make_key(2);
  const auto device = generate_device_key();

  ASSERT_TRUE(device.has_value());

  ReceiptHeader header = make_header(2);
  header.version = k_receipt_version + 1;

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  const auto opened = open_receipt_key(receipt, device->private_key);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::receipt_version_unsupported);
}

TEST(Receipt, RejectsTamperedFields) {
  const SymmetricKey purchase_key = make_key(3);
  const auto device = generate_device_key();
  const ReceiptHeader header = make_header(3);

  ASSERT_TRUE(device.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  ReceiptHeader purchase_changed = header;
  purchase_changed.purchase_id = header.purchase_id + 1;

  ReceiptHeader user_changed = header;
  user_changed.user_id.front() = static_cast<std::uint8_t>(user_changed.user_id.front() ^ 1U);

  ReceiptHeader purchase_date_changed = header;
  purchase_date_changed.purchased_at = header.purchased_at + 1;

  ReceiptHeader issue_date_changed = header;
  issue_date_changed.issued_at = header.issued_at + 1;

  for (const ReceiptHeader &changed : {purchase_changed, user_changed, purchase_date_changed, issue_date_changed}) {
    const Receipt receipt{.header = changed, .wrapped_key = wrapped.value()};

    const auto opened = open_receipt_key(receipt, device->private_key);

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error(), CoreError::authentication_failed);
  }
}

TEST(Receipt, RejectsWrapOfAnotherPurchase) {
  const SymmetricKey purchase_key = make_key(4);
  const auto device = generate_device_key();

  ASSERT_TRUE(device.has_value());

  const ReceiptHeader first = make_header(4);
  const ReceiptHeader second = make_header(5);

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, first);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt moved{.header = second, .wrapped_key = wrapped.value()};

  const auto opened = open_receipt_key(moved, device->private_key);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST(Receipt, RejectsForeignDevice) {
  const SymmetricKey purchase_key = make_key(5);
  const auto device = generate_device_key();
  const auto foreign = generate_device_key();
  const ReceiptHeader header = make_header(6);

  ASSERT_TRUE(device.has_value());
  ASSERT_TRUE(foreign.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  const auto opened = open_receipt_key(receipt, foreign->private_key);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

} // namespace
