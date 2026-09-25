#pragma once

#include <dgds/client/http/server_trust.h>
#include <dgds/client/models/endpoint.h>
#include <dgds/client/ports/api_transport.h>
#include <dgds/core/models/errors.h>

#include <utility>

namespace dgds::client {

class HttpTransport : public ApiTransport {
public:
  HttpTransport(Endpoint endpoint, ServerTrust trust) : m_endpoint(std::move(endpoint)), m_trust(std::move(trust)) {}

  [[nodiscard]] Result<UserAccount> register_user(Content name) override;
  [[nodiscard]] Result<Credentials> log_in(Content name) override;
  [[nodiscard]] Result<PublicationSummaries> catalog() override;
  [[nodiscard]] Result<PublicationId> publish(const Credentials &credentials, const PublicationDraft &draft,
                                              const AuthorPublicKey &author_key, const Signature &signature) override;
  [[nodiscard]] Result<Receipt> buy(const Credentials &credentials, const PublicationId &publication_id,
                                    const DevicePublicKey &device_key) override;
  [[nodiscard]] Result<PurchaseSummaries> purchases(const Credentials &credentials) override;
  [[nodiscard]] Result<Receipt> restore_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                                const DevicePublicKey &device_key) override;
  [[nodiscard]] Result<ContentIdentity> context_identity(const Credentials &credentials,
                                                         const PurchaseId &context_id) override;
  [[nodiscard]] Result<Package> fetch_package(const Credentials &credentials, const PurchaseId &purchase_id,
                                              const DevicePublicKey &device_key) override;

private:
  Endpoint m_endpoint;
  ServerTrust m_trust;
};

} // namespace dgds::client
