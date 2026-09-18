#pragma once

#include <dgds/core/models/device_key.h>
#include <dgds/core/models/receipt.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/symmetric_key.h>

namespace dgds::client {

using core::DevicePublicKey;
using core::Receipt;
using core::Result;
using core::SymmetricKey;

class DeviceKey {
public:
  DeviceKey() = default;
  DeviceKey(const DeviceKey &) = delete;
  DeviceKey &operator=(const DeviceKey &) = delete;
  DeviceKey(DeviceKey &&) = delete;
  DeviceKey &operator=(DeviceKey &&) = delete;
  virtual ~DeviceKey() = default;

  [[nodiscard]] virtual Result<DevicePublicKey> public_key() = 0;
  [[nodiscard]] virtual Result<SymmetricKey> open_receipt_key(const Receipt &receipt) = 0;
};

} // namespace dgds::client
