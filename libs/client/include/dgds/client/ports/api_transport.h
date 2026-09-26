#pragma once

#include <dgds/core/models/author.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/device.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/protocol.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>

namespace dgds::client {

using core::AuthorPublicKey;
using core::Content;
using core::ContentIdentity;
using core::Credentials;
using core::DevicePublicKey;
using core::Package;
using core::PublicationDraft;
using core::PublicationId;
using core::PublicationSummaries;
using core::PublicationSummary;
using core::PurchaseId;
using core::PurchaseSummaries;
using core::PurchaseSummary;
using core::Receipt;
using core::Result;
using core::Signature;
using core::UserAccount;

class ApiTransport {
public:
  ApiTransport() = default;
  ApiTransport(const ApiTransport &) = delete;
  ApiTransport &operator=(const ApiTransport &) = delete;
  ApiTransport(ApiTransport &&) = delete;
  ApiTransport &operator=(ApiTransport &&) = delete;
  virtual ~ApiTransport() = default;

  [[nodiscard]] virtual Result<UserAccount> register_user(Content name) = 0;
  [[nodiscard]] virtual Result<Credentials> log_in(Content name) = 0;
  [[nodiscard]] virtual Result<PublicationSummaries> catalog(std::size_t offset = 0,
                                                             std::size_t limit = core::k_default_page_size) = 0;
  [[nodiscard]] virtual Result<PublicationId> publish(const Credentials &credentials, const PublicationDraft &draft,
                                                      const AuthorPublicKey &author_key,
                                                      const Signature &signature) = 0;
  [[nodiscard]] virtual Result<Receipt> buy(const Credentials &credentials, const PublicationId &publication_id,
                                            const DevicePublicKey &device_key) = 0;
  [[nodiscard]] virtual Result<PurchaseSummaries> purchases(const Credentials &credentials, std::size_t offset = 0,
                                                            std::size_t limit = core::k_default_page_size) = 0;
  [[nodiscard]] virtual Result<Receipt> restore_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                                        const DevicePublicKey &device_key) = 0;
  [[nodiscard]] virtual Result<ContentIdentity> context_identity(const Credentials &credentials,
                                                                 const PurchaseId &context_id) = 0;
  [[nodiscard]] virtual Result<Package> fetch_package(const Credentials &credentials, const PurchaseId &purchase_id,
                                                      const DevicePublicKey &device_key) = 0;
};

} // namespace dgds::client
