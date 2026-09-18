#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/device_key.h>
#include <dgds/core/models/publication_id.h>
#include <dgds/core/models/publication_record.h>
#include <dgds/core/models/purchase_id.h>
#include <dgds/core/models/purchase_record.h>
#include <dgds/core/models/receipt_record.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/user_account.h>
#include <dgds/core/models/user_id.h>

#include <cstddef>
#include <vector>

namespace dgds::core {

using PublicationRecords = std::vector<PublicationRecord>;
using PurchaseRecords = std::vector<PurchaseRecord>;

class MetadataRegistry {
public:
  MetadataRegistry() = default;
  MetadataRegistry(const MetadataRegistry &) = delete;
  MetadataRegistry &operator=(const MetadataRegistry &) = delete;
  MetadataRegistry(MetadataRegistry &&) = delete;
  MetadataRegistry &operator=(MetadataRegistry &&) = delete;
  virtual ~MetadataRegistry() = default;

  [[nodiscard]] virtual Result<void> add_user(const UserAccount &account) = 0;
  [[nodiscard]] virtual Result<UserAccount> find_user(const UserId &user_id) = 0;
  [[nodiscard]] virtual Result<UserAccount> find_user_by_name(Content name) = 0;

  [[nodiscard]] virtual Result<void> add_publication(const PublicationRecord &publication) = 0;
  [[nodiscard]] virtual Result<PublicationRecord> find_publication(const PublicationId &publication_id) = 0;
  [[nodiscard]] virtual Result<PublicationRecords> publications() = 0;
  [[nodiscard]] virtual Result<PublicationRecords> publications_of_author(const UserId &author_id) = 0;

  [[nodiscard]] virtual Result<void> add_purchase(const PurchaseRecord &purchase) = 0;
  [[nodiscard]] virtual Result<PurchaseRecord> find_purchase(const PurchaseId &purchase_id) = 0;
  [[nodiscard]] virtual Result<PurchaseRecord> find_purchase_of(const UserId &user_id,
                                                                const PublicationId &publication_id) = 0;
  [[nodiscard]] virtual Result<PurchaseRecords> purchases_of_user(const UserId &user_id) = 0;
  [[nodiscard]] virtual Result<std::size_t> purchase_count(const PublicationId &publication_id) = 0;

  [[nodiscard]] virtual Result<void> add_receipt(const ReceiptRecord &record) = 0;
  [[nodiscard]] virtual Result<ReceiptRecord> find_receipt(const PurchaseId &purchase_id,
                                                           const DevicePublicKey &device_key) = 0;
};

} // namespace dgds::core
