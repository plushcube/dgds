#include <dgds/stubs/device_key/file_device_key.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/receipt.h>
#include <dgds/stubs/support/file_storage.h>

#include <expected>

namespace dgds::stubs {

core::Result<core::DevicePrivateKey> FileDeviceKey::private_key() const {
  const auto stored = load_or_create_secret(m_path);

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  return core::DevicePrivateKey(stored.value());
}

client::Result<client::DevicePublicKey> FileDeviceKey::public_key() {
  const auto key = private_key();

  if (!key.has_value()) {
    return std::unexpected(key.error());
  }

  return core::derive_device_public_key(key.value());
}

client::Result<client::SymmetricKey> FileDeviceKey::open_receipt_key(const client::Receipt &receipt) {
  const auto key = private_key();

  if (!key.has_value()) {
    return std::unexpected(key.error());
  }

  return core::open_receipt_key(receipt, key.value());
}

} // namespace dgds::stubs
