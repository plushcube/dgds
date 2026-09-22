#pragma once

#include <dgds/client/ports/device_key.h>

#include <filesystem>
#include <utility>

namespace dgds::stubs {

class FileDeviceKey : public client::DeviceKey {
public:
  explicit FileDeviceKey(std::filesystem::path path) : m_path(std::move(path)) {}

  [[nodiscard]] client::Result<client::DevicePublicKey> public_key() override;
  [[nodiscard]] client::Result<client::SymmetricKey> open_receipt_key(const client::Receipt &receipt) override;

private:
  [[nodiscard]] core::Result<core::DevicePrivateKey> private_key() const;

  std::filesystem::path m_path;
};

} // namespace dgds::stubs
