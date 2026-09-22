#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>

namespace dgds::stubs {

[[nodiscard]] core::Result<core::ContentBuffer> encode_user(const core::UserAccount &account);
[[nodiscard]] core::Result<core::UserAccount> decode_user(core::Content data);

[[nodiscard]] core::Result<core::ContentBuffer> encode_publication(const core::PublicationRecord &publication);
[[nodiscard]] core::Result<core::PublicationRecord> decode_publication(core::Content data);

[[nodiscard]] core::Result<core::ContentBuffer> encode_purchase(const core::PurchaseRecord &purchase);
[[nodiscard]] core::Result<core::PurchaseRecord> decode_purchase(core::Content data);

[[nodiscard]] core::Result<core::ContentBuffer> encode_receipt_record(const core::ReceiptRecord &record);
[[nodiscard]] core::Result<core::ReceiptRecord> decode_receipt_record(core::Content data);

} // namespace dgds::stubs
