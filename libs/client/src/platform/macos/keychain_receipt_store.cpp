#include <dgds/client/platform/macos/keychain_receipt_store.h>

#include <Security/Security.h>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace dgds::client {
namespace {

template <typename Reference> class CfHandle {
public:
  explicit CfHandle(Reference reference = nullptr) : p_reference(reference) {}
  CfHandle(const CfHandle &) = delete;
  CfHandle &operator=(const CfHandle &) = delete;
  CfHandle(CfHandle &&) = delete;
  CfHandle &operator=(CfHandle &&) = delete;
  ~CfHandle() {
    if (p_reference != nullptr) {
      CFRelease(p_reference);
    }
  }

  [[nodiscard]] Reference get() const { return p_reference; }

private:
  Reference p_reference;
};

CfHandle<CFStringRef> make_string(std::string_view text) {
  return CfHandle<CFStringRef>(
      CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(text.data()),
                              static_cast<CFIndex>(text.size()), kCFStringEncodingUTF8, false));
}

std::string account_of(const core::PurchaseId &purchase_id) { return std::to_string(purchase_id); }

core::Result<core::ContentBuffer> read_blob(const std::string &service, const std::string &account) {
  const auto service_name = make_string(service);
  const auto account_name = make_string(account);

  const void *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecReturnData, kSecMatchLimit};
  const void *values[] = {kSecClassGenericPassword, service_name.get(), account_name.get(), kCFBooleanTrue,
                          kSecMatchLimitOne};

  const CfHandle<CFDictionaryRef> query(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  CFTypeRef found = nullptr;
  const OSStatus status = SecItemCopyMatching(query.get(), &found);

  if (status == errSecItemNotFound) {
    return std::unexpected(core::CoreError::receipt_not_found);
  }

  if (status != errSecSuccess) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const CfHandle<CFTypeRef> data(found);

  if (CFGetTypeID(data.get()) != CFDataGetTypeID()) {
    return std::unexpected(core::CoreError::receipt_malformed);
  }

  const CFDataRef bytes = static_cast<CFDataRef>(data.get());

  return core::ContentBuffer(reinterpret_cast<const char *>(CFDataGetBytePtr(bytes)),
                             static_cast<std::size_t>(CFDataGetLength(bytes)));
}

core::Result<void> write_blob(const std::string &service, const std::string &account, core::Content blob) {
  const auto service_name = make_string(service);
  const auto account_name = make_string(account);
  const CfHandle<CFDataRef> data(CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(blob.data()),
                                              static_cast<CFIndex>(blob.size())));

  const void *keys[] = {kSecClass, kSecAttrService, kSecAttrAccount, kSecAttrAccessible, kSecValueData};
  const void *values[] = {kSecClassGenericPassword, service_name.get(), account_name.get(),
                          kSecAttrAccessibleWhenUnlockedThisDeviceOnly, data.get()};

  const CfHandle<CFDictionaryRef> item(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

  const OSStatus added = SecItemAdd(item.get(), nullptr);

  if (added == errSecSuccess) {
    return {};
  }

  if (added != errSecDuplicateItem) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const void *query_keys[] = {kSecClass, kSecAttrService, kSecAttrAccount};
  const void *query_values[] = {kSecClassGenericPassword, service_name.get(), account_name.get()};

  const CfHandle<CFDictionaryRef> query(CFDictionaryCreate(kCFAllocatorDefault, query_keys, query_values, 3,
                                                           &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks));

  const void *update_keys[] = {kSecValueData};
  const void *update_values[] = {data.get()};

  const CfHandle<CFDictionaryRef> replacement(CFDictionaryCreate(kCFAllocatorDefault, update_keys, update_values, 1,
                                                                 &kCFTypeDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks));

  if (SecItemUpdate(query.get(), replacement.get()) != errSecSuccess) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return {};
}

} // namespace

Result<void> KeychainReceiptStore::save(const PurchaseId &purchase_id, Content blob) {
  return write_blob(m_service, account_of(purchase_id), blob);
}

Result<ContentBuffer> KeychainReceiptStore::load(const PurchaseId &purchase_id) {
  return read_blob(m_service, account_of(purchase_id));
}

} // namespace dgds::client
