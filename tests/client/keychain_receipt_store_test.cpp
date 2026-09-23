#include <dgds/client/platform/macos/keychain_receipt_store.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/core/envelope/receipt.h>

#include <Security/Security.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using dgds::client::KeychainReceiptStore;
using dgds::core::ContentBuffer;
using dgds::core::CoreError;
using dgds::core::encode_receipt;
using dgds::core::generate_device_key;
using dgds::core::generate_key;
using dgds::core::k_receipt_version;
using dgds::core::PurchaseId;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::UserId;
using dgds::core::wrap_receipt_key;

CFStringRef string_ref(std::string_view text) {
  return CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(text.data()),
                                 static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false);
}

OSStatus probe_item(const std::string &service, const std::string &account) {
  const CFStringRef service_name = string_ref(service);
  const CFStringRef account_name = string_ref(account);

  const void *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecMatchLimit};
  const void *values[] = {kSecClassGenericPassword, service_name, account_name, kSecMatchLimitOne};

  const CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 4, &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);

  CFTypeRef found = nullptr;
  const OSStatus status = SecItemCopyMatching(query, &found);

  if (found != nullptr) {
    CFRelease(found);
  }

  CFRelease(query);
  CFRelease(account_name);
  CFRelease(service_name);

  return status;
}

void remove_item(const std::string &service, const std::string &account) {
  const CFStringRef service_name = string_ref(service);
  const CFStringRef account_name = string_ref(account);

  const void *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount};
  const void *values[] = {kSecClassGenericPassword, service_name, account_name};

  const CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);

  SecItemDelete(query);

  CFRelease(query);
  CFRelease(account_name);
  CFRelease(service_name);
}

UserId make_user_id() {
  UserId user_id{};

  for (std::size_t index = 0; index < user_id.size(); ++index) {
    user_id[index] = static_cast<std::uint8_t>(index * 7 + 3);
  }

  return user_id;
}

ContentBuffer make_receipt_blob(const PurchaseId &purchase_id) {
  const auto device = generate_device_key();
  const auto purchase_key = generate_key();

  EXPECT_TRUE(device.has_value());
  EXPECT_TRUE(purchase_key.has_value());

  const ReceiptHeader header{.version = k_receipt_version,
                             .purchase_id = purchase_id,
                             .user_id = make_user_id(),
                             .purchased_at = 1700000500,
                             .issued_at = 1700000600};
  const auto wrapped = wrap_receipt_key(purchase_key.value(), device->public_key, header);

  EXPECT_TRUE(wrapped.has_value());

  const auto encoded = encode_receipt(Receipt{.header = header, .wrapped_key = wrapped.value()});
  EXPECT_TRUE(encoded.has_value());

  return encoded.value_or(ContentBuffer{});
}

class KeychainReceiptStoreTest : public ::testing::Test {
protected:
  KeychainReceiptStoreTest()
      : m_service("dgds-test-receipts-" + std::to_string(::getpid())),
        m_purchase_id(static_cast<PurchaseId>(::getpid()) * 1000 + counter++) {}

  void TearDown() override { remove_item(m_service, account()); }

  [[nodiscard]] std::string account() const { return std::to_string(m_purchase_id); }

  std::string m_service;
  PurchaseId m_purchase_id;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(KeychainReceiptStoreTest, KeepsReceiptAcrossInstances) {
  const ContentBuffer blob = make_receipt_blob(m_purchase_id);
  ASSERT_FALSE(blob.empty());

  {
    KeychainReceiptStore store{m_service};
    ASSERT_TRUE(store.save(m_purchase_id, blob).has_value());
  }

  EXPECT_EQ(probe_item(m_service, account()), errSecSuccess);

  KeychainReceiptStore other{m_service};
  const auto loaded = other.load(m_purchase_id);

  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded.value(), blob);
}

TEST_F(KeychainReceiptStoreTest, ReplacesReceiptOnRepeatSave) {
  const ContentBuffer first = make_receipt_blob(m_purchase_id);
  const ContentBuffer second = make_receipt_blob(m_purchase_id);
  ASSERT_NE(first, second);

  KeychainReceiptStore store{m_service};
  ASSERT_TRUE(store.save(m_purchase_id, first).has_value());
  ASSERT_TRUE(store.save(m_purchase_id, second).has_value());

  const auto loaded = store.load(m_purchase_id);

  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded.value(), second);
}

TEST_F(KeychainReceiptStoreTest, ReportsMissingReceipt) {
  KeychainReceiptStore store{m_service};

  const auto loaded = store.load(m_purchase_id);

  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error(), CoreError::receipt_not_found);
}

} // namespace
