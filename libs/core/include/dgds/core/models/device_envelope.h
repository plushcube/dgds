#pragma once

#include <dgds/core/models/device_key.h>
#include <dgds/core/models/sealed_content.h>

namespace dgds::core {

struct DeviceEnvelope {
  DevicePublicKey ephemeral_key;
  SealedContent wrapped;
};

} // namespace dgds::core
