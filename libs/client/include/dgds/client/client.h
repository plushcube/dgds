#pragma once

#include <dgds/client/ports/api_transport.h>
#include <dgds/client/ports/device_key.h>
#include <dgds/client/ports/receipt_store.h>

#include <dgds/core/crypto/secure_buffer.h>
#include <dgds/core/models/author.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/device.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>

namespace dgds::client {

using core::AuthorPublicKey;
using core::Content;
using core::Credentials;
using core::DevicePublicKey;
using core::PublicationDraft;
using core::PublicationId;
using core::PublicationSummaryPage;
using core::PurchaseId;
using core::PurchaseSummaryPage;
using core::Receipt;
using core::Result;
using core::SecureBuffer;
using core::Signature;
using core::UserAccount;

class ApiClient {
public:
  ApiClient(ApiTransport &transport, DeviceKey &device_key, ReceiptStore &receipts)
      : m_transport(transport), m_device_key(device_key), m_receipts(receipts) {}

  [[nodiscard]] Result<UserAccount> register_user(Content name) { return m_transport.register_user(name); }

  [[nodiscard]] Result<Credentials> log_in(Content name) { return m_transport.log_in(name); }

  [[nodiscard]] Result<PublicationSummaryPage> catalog(std::size_t offset = 0,
                                                       std::size_t limit = core::k_default_page_size) {
    return m_transport.catalog(offset, limit);
  }

  [[nodiscard]] Result<PublicationId> publish(const Credentials &credentials, const PublicationDraft &draft,
                                              const AuthorPublicKey &author_key, const Signature &signature) {
    return m_transport.publish(credentials, draft, author_key, signature);
  }

  [[nodiscard]] Result<PurchaseSummaryPage> purchases(const Credentials &credentials, std::size_t offset = 0,
                                                      std::size_t limit = core::k_default_page_size) {
    return m_transport.purchases(credentials, offset, limit);
  }

  [[nodiscard]] Result<Receipt> buy(const Credentials &credentials, const PublicationId &publication_id);

  [[nodiscard]] Result<SecureBuffer> fetch_content(const Credentials &credentials, const PurchaseId &purchase_id);

private:
  [[nodiscard]] Result<void> save_receipt(const Receipt &receipt);

  [[nodiscard]] Result<Receipt> stored_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                               const DevicePublicKey &device_key);

  ApiTransport &m_transport;
  DeviceKey &m_device_key;
  ReceiptStore &m_receipts;
};

} // namespace dgds::client
