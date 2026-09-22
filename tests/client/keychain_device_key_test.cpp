#include <dgds/client/platform/macos/keychain_device_key.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/core/envelope/package.h>
#include <dgds/core/envelope/receipt.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/signature/author_signature.h>

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

using dgds::client::KeychainDeviceKey;
using dgds::core::as_content;
using dgds::core::content_identity;
using dgds::core::ContentBuffer;
using dgds::core::encrypt;
using dgds::core::generate_author_key;
using dgds::core::generate_key;
using dgds::core::k_package_version;
using dgds::core::k_receipt_version;
using dgds::core::k_signature_algorithm;
using dgds::core::open_package;
using dgds::core::Package;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::Result;
using dgds::core::sign_author;
using dgds::core::SymmetricKey;
using dgds::core::UserId;
using dgds::core::wrap_key;
using dgds::core::wrap_receipt_key;

constexpr std::string_view k_text = "секретный текст публикации";
constexpr std::string_view k_author = "автор";

CFStringRef service_ref(const std::string &service) {
  return CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(service.data()),
                                 static_cast<CFIndex>(service.size()), kCFStringEncodingUTF8, false);
}

CFStringRef account_ref() {
  constexpr std::string_view k_account = "device-key";

  return CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(k_account.data()),
                                 static_cast<CFIndex>(k_account.size()), kCFStringEncodingUTF8, false);
}

OSStatus probe_service(const std::string &service) {
  const CFStringRef service_name = service_ref(service);
  const CFStringRef account_name = account_ref();

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

void remove_service(const std::string &service) {
  const CFStringRef service_name = service_ref(service);
  const CFStringRef account_name = account_ref();

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
    user_id[index] = static_cast<std::uint8_t>(index * 3 + 1);
  }

  return user_id;
}

Result<Package> make_package(const SymmetricKey &purchase_key) {
  const auto identity = content_identity(k_text);

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  const auto file_key = generate_key();

  if (!file_key.has_value()) {
    return std::unexpected(file_key.error());
  }

  const auto author = generate_author_key();

  if (!author.has_value()) {
    return std::unexpected(author.error());
  }

  const auto content = encrypt(k_text, file_key.value(), as_content(identity.value()));

  if (!content.has_value()) {
    return std::unexpected(content.error());
  }

  const auto wrapped = wrap_key(file_key.value(), purchase_key, as_content(identity.value()));

  if (!wrapped.has_value()) {
    return std::unexpected(wrapped.error());
  }

  const auto signature = sign_author(identity.value(), k_author, author->private_key);

  if (!signature.has_value()) {
    return std::unexpected(signature.error());
  }

  return Package{.version = k_package_version,
                 .content = content.value(),
                 .wrapped_blob_key = wrapped.value(),
                 .identity = identity.value(),
                 .author_key = author->public_key,
                 .author_name = ContentBuffer(k_author),
                 .signature_algorithm = k_signature_algorithm,
                 .signature = signature.value()};
}

class KeychainDeviceKeyTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_service = "dgds.test." + std::to_string(::getpid()) + "." + std::to_string(counter++);
    m_other_service = m_service + ".other";

    const OSStatus status = probe_service(m_service);

    if (status != errSecItemNotFound && status != errSecSuccess) {
      GTEST_SKIP() << "Keychain недоступен в этом окружении, код " << status;
    }
  }

  void TearDown() override {
    remove_service(m_service);
    remove_service(m_other_service);
  }

  std::string m_service;
  std::string m_other_service;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(KeychainDeviceKeyTest, KeepsKeyAcrossInstances) {
  KeychainDeviceKey first(m_service);
  KeychainDeviceKey second(m_service);

  const auto initial = first.public_key();
  const auto repeated = second.public_key();

  ASSERT_TRUE(initial.has_value());
  ASSERT_TRUE(repeated.has_value());
  EXPECT_EQ(initial.value(), repeated.value());
}

TEST_F(KeychainDeviceKeyTest, OpensReceiptAndDecryptsPackage) {
  KeychainDeviceKey device(m_service);

  const auto purchase_key = generate_key();
  ASSERT_TRUE(purchase_key.has_value());

  const auto public_key = device.public_key();
  ASSERT_TRUE(public_key.has_value());

  const ReceiptHeader header{.version = k_receipt_version,
                             .purchase_id = 42,
                             .user_id = make_user_id(),
                             .purchased_at = 1700000000,
                             .issued_at = 1700000100};

  const auto wrapped_key = wrap_receipt_key(purchase_key.value(), public_key.value(), header);
  ASSERT_TRUE(wrapped_key.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped_key.value()};
  const auto package = make_package(purchase_key.value());
  ASSERT_TRUE(package.has_value());

  const auto receipt_key = device.open_receipt_key(receipt);

  ASSERT_TRUE(receipt_key.has_value());
  EXPECT_TRUE(receipt_key->equals(purchase_key.value()));

  const auto content = open_package(package.value(), receipt_key.value());

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->view(), k_text);
}

TEST_F(KeychainDeviceKeyTest, RejectsReceiptOfAnotherDevice) {
  KeychainDeviceKey owner(m_service);
  KeychainDeviceKey stranger(m_other_service);

  const auto purchase_key = generate_key();
  ASSERT_TRUE(purchase_key.has_value());

  const auto owner_key = owner.public_key();
  ASSERT_TRUE(owner_key.has_value());

  const ReceiptHeader header{.version = k_receipt_version,
                             .purchase_id = 43,
                             .user_id = make_user_id(),
                             .purchased_at = 1700000000,
                             .issued_at = 1700000100};

  const auto wrapped_key = wrap_receipt_key(purchase_key.value(), owner_key.value(), header);
  ASSERT_TRUE(wrapped_key.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped_key.value()};

  const auto opened = stranger.open_receipt_key(receipt);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), dgds::core::CoreError::authentication_failed);
}

} // namespace
