#include <dgds/client/platform/macos/keychain_device_key.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/receipt.h>

#include <Security/Security.h>

#include <cstddef>
#include <cstring>
#include <expected>
#include <string>
#include <string_view>

namespace dgds::client {
namespace {

constexpr const char *k_account = "device-key";
constexpr std::size_t k_secret_size = core::k_device_key_size;

template <typename Reference> class CfHandle {
public:
  explicit CfHandle(Reference reference = nullptr) : m_reference(reference) {}
  CfHandle(const CfHandle &) = delete;
  CfHandle &operator=(const CfHandle &) = delete;
  CfHandle(CfHandle &&) = delete;
  CfHandle &operator=(CfHandle &&) = delete;
  ~CfHandle() {
    if (m_reference != nullptr) {
      CFRelease(m_reference);
    }
  }

  [[nodiscard]] Reference get() const { return m_reference; }

private:
  Reference m_reference;
};

CfHandle<CFStringRef> make_string(std::string_view text) {
  return CfHandle<CFStringRef>(
      CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(text.data()),
                              static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false));
}

core::Result<core::SecretBytes<k_secret_size>> read_secret(const std::string &service) {
  const auto service_name = make_string(service);
  const auto account_name = make_string(k_account);

  const void *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit};
  const void *values[] = {kSecClassGenericPassword, service_name.get(), account_name.get(), kCFBooleanTrue,
                          kSecMatchLimitOne};

  const CfHandle<CFDictionaryRef> query(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  CFTypeRef found = nullptr;
  const OSStatus status = SecItemCopyMatching(query.get(), &found);

  if (status == errSecItemNotFound) {
    return std::unexpected(core::CoreError::key_not_found);
  }

  if (status != errSecSuccess) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const CfHandle<CFTypeRef> data(found);

  if (CFGetTypeID(data.get()) != CFDataGetTypeID() ||
      CFDataGetLength(static_cast<CFDataRef>(data.get())) != static_cast<CFIndex>(k_secret_size)) {
    return std::unexpected(core::CoreError::key_size_mismatch);
  }

  core::SecretBytes<k_secret_size> secret;

  std::memcpy(secret.data(), CFDataGetBytePtr(static_cast<CFDataRef>(data.get())), k_secret_size);

  return secret;
}

core::Result<void> write_secret(const std::string &service, const core::SecretBytes<k_secret_size> &secret) {
  const auto service_name = make_string(service);
  const auto account_name = make_string(k_account);
  const CfHandle<CFDataRef> data(CFDataCreate(kCFAllocatorDefault, secret.data(), static_cast<CFIndex>(secret.size())));

  const void *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecAttrAccessible, kSecValueData};
  const void *values[] = {kSecClassGenericPassword, service_name.get(), account_name.get(),
                          kSecAttrAccessibleWhenUnlockedThisDeviceOnly, data.get()};

  const CfHandle<CFDictionaryRef> item(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  const OSStatus status = SecItemAdd(item.get(), nullptr);

  if (status == errSecDuplicateItem) {
    return {};
  }

  if (status != errSecSuccess) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return {};
}

} // namespace

core::Result<core::DevicePrivateKey> KeychainDeviceKey::private_key() const {
  auto existing = read_secret(m_service);

  if (existing.has_value()) {
    return core::DevicePrivateKey(existing.value());
  }

  if (existing.error() != core::CoreError::key_not_found) {
    return std::unexpected(existing.error());
  }

  const auto generated = core::generate_device_key();

  if (!generated.has_value()) {
    return std::unexpected(generated.error());
  }

  const auto stored = write_secret(m_service, generated->private_key);

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  return read_secret(m_service);
}

Result<DevicePublicKey> KeychainDeviceKey::public_key() {
  const auto key = private_key();

  if (!key.has_value()) {
    return std::unexpected(key.error());
  }

  return core::derive_device_public_key(key.value());
}

Result<SymmetricKey> KeychainDeviceKey::open_receipt_key(const Receipt &receipt) {
  const auto key = private_key();

  if (!key.has_value()) {
    return std::unexpected(key.error());
  }

  return core::open_receipt_key(receipt, key.value());
}

} // namespace dgds::client
