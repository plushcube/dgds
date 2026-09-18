#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/credentials.h>
#include <dgds/core/models/device_key.h>
#include <dgds/core/models/package.h>
#include <dgds/core/models/publication_id.h>
#include <dgds/core/models/publication_summary.h>
#include <dgds/core/models/purchase_id.h>
#include <dgds/core/models/purchase_summary.h>
#include <dgds/core/models/receipt.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/user_account.h>

#include <vector>

namespace dgds::client {

using core::Content;
using core::Credentials;
using core::DevicePublicKey;
using core::Package;
using core::PublicationId;
using core::PublicationSummary;
using core::PurchaseId;
using core::PurchaseSummary;
using core::Receipt;
using core::Result;
using core::UserAccount;

using PublicationSummaries = std::vector<PublicationSummary>;
using PurchaseSummaries = std::vector<PurchaseSummary>;

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
  [[nodiscard]] virtual Result<PublicationSummaries> catalog() = 0;
  [[nodiscard]] virtual Result<Receipt> buy(const Credentials &credentials, const PublicationId &publication_id,
                                            const DevicePublicKey &device_key) = 0;
  [[nodiscard]] virtual Result<PurchaseSummaries> purchases(const Credentials &credentials) = 0;
  [[nodiscard]] virtual Result<Receipt> restore_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                                        const DevicePublicKey &device_key) = 0;
  [[nodiscard]] virtual Result<Package> fetch_package(const Credentials &credentials,
                                                      const PurchaseId &purchase_id) = 0;
};

} // namespace dgds::client
