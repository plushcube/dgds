#pragma once

#include <dgds/client/ports/device_key.h>

#include <string>
#include <utility>

namespace dgds::client {

class KeychainDeviceKey : public DeviceKey {
public:
  explicit KeychainDeviceKey(std::string service) : m_service(std::move(service)) {}

  [[nodiscard]] Result<DevicePublicKey> public_key() override;
  [[nodiscard]] Result<SymmetricKey> open_receipt_key(const Receipt &receipt) override;

private:
  [[nodiscard]] core::Result<core::DevicePrivateKey> private_key() const;

  std::string m_service;
};

} // namespace dgds::client
