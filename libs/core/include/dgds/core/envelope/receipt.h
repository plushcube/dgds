#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/device.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/errors.h>

namespace dgds::core {

[[nodiscard]] Result<DeviceEnvelope> wrap_receipt_key(const SymmetricKey &purchase_key,
                                                      const DevicePublicKey &device_key, const ReceiptHeader &header);
[[nodiscard]] Result<SymmetricKey> open_receipt_key(const Receipt &receipt, const DevicePrivateKey &device_key);

[[nodiscard]] Result<ContentBuffer> encode_receipt(const Receipt &receipt);
[[nodiscard]] Result<Receipt> decode_receipt(Content data);

} // namespace dgds::core
