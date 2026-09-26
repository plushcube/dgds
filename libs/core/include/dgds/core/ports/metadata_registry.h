#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/device.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>

#include <cstddef>

namespace dgds::core {

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
  [[nodiscard]] virtual Result<PublicationPage> publications(std::size_t offset, std::size_t limit) = 0;
  [[nodiscard]] virtual Result<PublicationPage> publications_of_author(const UserId &author_id, std::size_t offset,
                                                                       std::size_t limit) = 0;

  [[nodiscard]] virtual Result<void> add_purchase(const PurchaseRecord &purchase) = 0;
  [[nodiscard]] virtual Result<PurchaseRecord> find_purchase(const PurchaseId &purchase_id) = 0;
  [[nodiscard]] virtual Result<PurchaseRecord> find_purchase_of(const UserId &user_id,
                                                                const PublicationId &publication_id) = 0;
  [[nodiscard]] virtual Result<PurchasePage> purchases_of_user(const UserId &user_id, std::size_t offset,
                                                               std::size_t limit) = 0;
  [[nodiscard]] virtual Result<std::size_t> purchase_count(const PublicationId &publication_id) = 0;

  [[nodiscard]] virtual Result<void> save_receipt(const ReceiptRecord &record) = 0;
  [[nodiscard]] virtual Result<ReceiptRecord> find_receipt(const PurchaseId &purchase_id,
                                                           const DevicePublicKey &device_key) = 0;
};

} // namespace dgds::core
