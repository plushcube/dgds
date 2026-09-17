#include <dgds/core/envelope/receipt.h>

#include <dgds/core/envelope/device_wrap.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using dgds::core::Content;
using dgds::core::CoreError;
using dgds::core::decode_receipt;
using dgds::core::encode_receipt;
using dgds::core::generate_device_key;
using dgds::core::k_receipt_version;
using dgds::core::open_receipt_key;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::SymmetricKey;
using dgds::core::UserId;
using dgds::core::wrap_receipt_key;

constexpr std::size_t k_ciphertext_length_size = 4;

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

TEST(ReceiptCodec, RoundTripKeepsReceiptUsable) {
  const SymmetricKey purchase_key = make_key(4);
  const auto device = generate_device_key();
  const ReceiptHeader header = make_header(4);

  ASSERT_TRUE(device.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  const auto blob = encode_receipt(receipt);
  ASSERT_TRUE(blob.has_value());

  const auto decoded = decode_receipt(blob.value());
  ASSERT_TRUE(decoded.has_value());

  EXPECT_EQ(decoded->header.version, header.version);
  EXPECT_EQ(decoded->header.purchase_id, header.purchase_id);
  EXPECT_EQ(decoded->header.user_id, header.user_id);
  EXPECT_EQ(decoded->header.purchased_at, header.purchased_at);
  EXPECT_EQ(decoded->header.issued_at, header.issued_at);
  EXPECT_EQ(decoded->wrapped_key.ephemeral_key, receipt.wrapped_key.ephemeral_key);
  EXPECT_EQ(decoded->wrapped_key.wrapped.algorithm, receipt.wrapped_key.wrapped.algorithm);
  EXPECT_EQ(decoded->wrapped_key.wrapped.nonce, receipt.wrapped_key.wrapped.nonce);
  EXPECT_EQ(decoded->wrapped_key.wrapped.ciphertext, receipt.wrapped_key.wrapped.ciphertext);
  EXPECT_EQ(decoded->wrapped_key.wrapped.tag, receipt.wrapped_key.wrapped.tag);

  const auto opened = open_receipt_key(decoded.value(), device->private_key);

  ASSERT_TRUE(opened.has_value());
  EXPECT_TRUE(opened->equals(purchase_key));
}

TEST(ReceiptCodec, RejectsTruncatedData) {
  const SymmetricKey purchase_key = make_key(5);
  const auto device = generate_device_key();
  const ReceiptHeader header = make_header(5);

  ASSERT_TRUE(device.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const auto blob = encode_receipt(Receipt{.header = header, .wrapped_key = wrapped.value()});
  ASSERT_TRUE(blob.has_value());
  ASSERT_GT(blob->size(), 1U);

  for (std::size_t length = 0; length < blob->size(); ++length) {
    const auto decoded = decode_receipt(Content(blob->data(), length));

    ASSERT_FALSE(decoded.has_value()) << length;
    EXPECT_EQ(decoded.error(), CoreError::receipt_malformed) << length;
  }
}

TEST(ReceiptCodec, RejectsTrailingBytes) {
  const SymmetricKey purchase_key = make_key(6);
  const auto device = generate_device_key();
  const ReceiptHeader header = make_header(6);

  ASSERT_TRUE(device.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  auto blob = encode_receipt(Receipt{.header = header, .wrapped_key = wrapped.value()});
  ASSERT_TRUE(blob.has_value());

  blob->push_back('\0');

  const auto decoded = decode_receipt(blob.value());

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), CoreError::receipt_malformed);
}

TEST(ReceiptCodec, RejectsUnsupportedVersion) {
  const SymmetricKey purchase_key = make_key(7);
  const auto device = generate_device_key();

  ASSERT_TRUE(device.has_value());

  ReceiptHeader header = make_header(7);
  header.version = k_receipt_version + 1;

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const auto blob = encode_receipt(Receipt{.header = header, .wrapped_key = wrapped.value()});
  ASSERT_TRUE(blob.has_value());

  const auto decoded = decode_receipt(blob.value());

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), CoreError::receipt_version_unsupported);
}

TEST(ReceiptCodec, RejectsUnclaimedCiphertextLength) {
  const SymmetricKey purchase_key = make_key(8);
  const auto device = generate_device_key();
  const ReceiptHeader header = make_header(8);

  ASSERT_TRUE(device.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, device->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  auto blob = encode_receipt(receipt);
  ASSERT_TRUE(blob.has_value());
  ASSERT_GT(blob->size(), k_ciphertext_length_size + receipt.wrapped_key.wrapped.tag.size());

  const std::size_t length_offset = blob->size() - receipt.wrapped_key.wrapped.tag.size() -
                                    receipt.wrapped_key.wrapped.ciphertext.size() - k_ciphertext_length_size;

  for (std::size_t index = 0; index < k_ciphertext_length_size; ++index) {
    (*blob)[length_offset + index] = static_cast<char>(0xFF);
  }

  const auto decoded = decode_receipt(blob.value());

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), CoreError::receipt_malformed);
}

} // namespace
