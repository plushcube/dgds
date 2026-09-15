#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/receipt.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/symmetric_key.h>

namespace dgds::core {

[[nodiscard]] ContentBuffer receipt_associated_data(const ReceiptHeader &header);
[[nodiscard]] Result<SealedContent> wrap_receipt_key(const SymmetricKey &purchase_key, const SymmetricKey &device_key,
                                                     const ReceiptHeader &header);
[[nodiscard]] Result<SymmetricKey> open_receipt_key(const Receipt &receipt, const SymmetricKey &device_key);

} // namespace dgds::core
