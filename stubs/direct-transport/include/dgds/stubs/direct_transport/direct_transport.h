#pragma once

#include <dgds/client/ports/api_transport.h>
#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

#include <functional>
#include <utility>

namespace dgds::stubs {

using client::ApiTransport;
using client::AuthorPublicKey;
using client::Content;
using client::Credentials;
using client::DevicePublicKey;
using client::Package;
using client::PublicationDraft;
using client::PublicationId;
using client::PublicationSummaries;
using client::PurchaseId;
using client::PurchaseSummaries;
using client::Receipt;
using client::Result;
using client::Signature;
using client::UserAccount;

class DirectTransport : public ApiTransport {
public:
  using Clock = std::function<core::Timestamp()>;

  DirectTransport(server::UserService &users, server::SessionStore &sessions, server::CatalogService &catalog,
                  server::PublicationService &publications, server::PurchaseService &purchases,
                  server::DeliveryService &delivery, Clock clock)
      : m_users(users), m_sessions(sessions), m_catalog(catalog), m_publications(publications), m_purchases(purchases),
        m_delivery(delivery), m_clock(std::move(clock)) {}

  [[nodiscard]] Result<UserAccount> register_user(Content name) override { return m_users.register_user(name); }

  [[nodiscard]] Result<Credentials> log_in(Content name) override { return m_users.log_in(name); }

  [[nodiscard]] Result<PublicationSummaries> catalog() override { return m_catalog.catalog(); }

  [[nodiscard]] Result<PublicationId> publish(const Credentials &credentials, const PublicationDraft &draft,
                                              const AuthorPublicKey &author_key, const Signature &signature) override;

  [[nodiscard]] Result<Receipt> buy(const Credentials &credentials, const PublicationId &publication_id,
                                    const DevicePublicKey &device_key) override;

  [[nodiscard]] Result<PurchaseSummaries> purchases(const Credentials &credentials) override;

  [[nodiscard]] Result<Receipt> restore_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                                const DevicePublicKey &device_key) override;

  [[nodiscard]] Result<Package> fetch_package(const Credentials &credentials, const PurchaseId &purchase_id,
                                              const DevicePublicKey &device_key) override;

private:
  [[nodiscard]] Result<core::UserId> authorized(const Credentials &credentials);

  server::UserService &m_users;
  server::SessionStore &m_sessions;
  server::CatalogService &m_catalog;
  server::PublicationService &m_publications;
  server::PurchaseService &m_purchases;
  server::DeliveryService &m_delivery;
  Clock m_clock;
};

} // namespace dgds::stubs
