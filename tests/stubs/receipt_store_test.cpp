#include <dgds/stubs/device_key/file_device_key.h>
#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <dgds/core/envelope/receipt.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace {

using dgds::core::CoreError;
using dgds::core::decode_receipt;
using dgds::core::encode_receipt;
using dgds::core::k_receipt_version;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::SymmetricKey;
using dgds::core::UserId;
using dgds::core::wrap_receipt_key;
using dgds::stubs::FileDeviceKey;
using dgds::stubs::FileReceiptStore;

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

class ReceiptStoreTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_root = std::filesystem::temp_directory_path() /
             ("dgds-receipt-store-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(m_root);
  }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(ReceiptStoreTest, SavesAndLoadsUsableReceipt) {
  FileDeviceKey device(m_root / "device.key");
  FileReceiptStore store(m_root);
  const SymmetricKey purchase_key = make_key(3);
  const ReceiptHeader header = make_header(3);

  const auto public_key = device.public_key();
  ASSERT_TRUE(public_key.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, public_key.value(), header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  const auto blob = encode_receipt(receipt);
  ASSERT_TRUE(blob.has_value());

  const auto saved = store.save(header.purchase_id, blob.value());
  ASSERT_TRUE(saved.has_value());

  const auto loaded = store.load(header.purchase_id);
  ASSERT_TRUE(loaded.has_value());

  const auto decoded = decode_receipt(loaded.value());
  ASSERT_TRUE(decoded.has_value());

  const auto opened = device.open_receipt_key(decoded.value());

  ASSERT_TRUE(opened.has_value());
  EXPECT_TRUE(opened->equals(purchase_key));
}

TEST_F(ReceiptStoreTest, RejectsUnknownPurchase) {
  FileReceiptStore store(m_root);

  const auto loaded = store.load(7);

  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error(), CoreError::receipt_not_found);
}

TEST_F(ReceiptStoreTest, KeepsLatestReceiptOfPurchase) {
  constexpr dgds::core::PurchaseId k_purchase_id = 5;

  FileReceiptStore store(m_root);

  ASSERT_TRUE(store.save(k_purchase_id, "first").has_value());

  const auto replaced = store.save(k_purchase_id, "second");
  ASSERT_TRUE(replaced.has_value());

  const auto loaded = store.load(k_purchase_id);

  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded.value(), "second");
}

TEST_F(ReceiptStoreTest, RestrictsStoredReceiptToOwner) {
  FileReceiptStore store(m_root);

  const auto saved = store.save(9, "opaque");
  ASSERT_TRUE(saved.has_value());

  constexpr std::filesystem::perms k_shared = std::filesystem::perms::group_all | std::filesystem::perms::others_all;
  const auto permissions = std::filesystem::status(m_root / "9.receipt").permissions();

  EXPECT_EQ(permissions & k_shared, std::filesystem::perms::none);
}

} // namespace
