#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/device_envelope.h>
#include <dgds/core/models/device_key.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/symmetric_key.h>

namespace dgds::core {

[[nodiscard]] Result<DeviceKeyPair> generate_device_key();
[[nodiscard]] Result<DevicePublicKey> derive_device_public_key(const DevicePrivateKey &key);
[[nodiscard]] Result<DeviceEnvelope> seal_for_device(const SymmetricKey &key, const DevicePublicKey &device_key,
                                                     Content associated_data);
[[nodiscard]] Result<SymmetricKey> open_for_device(const DeviceEnvelope &envelope, const DevicePrivateKey &device_key,
                                                   Content associated_data);

} // namespace dgds::core
