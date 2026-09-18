#pragma once

#include <dgds/core/models/device_key.h>
#include <dgds/core/models/receipt.h>

namespace dgds::core {

struct ReceiptRecord {
  DevicePublicKey device_key;
  Receipt receipt;
};

} // namespace dgds::core
