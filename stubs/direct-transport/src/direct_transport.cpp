#include <dgds/stubs/direct_transport/direct_transport.h>

#include <expected>

namespace dgds::stubs {

Result<core::UserId> DirectTransport::authorized(const Credentials &credentials) {
  const auto user_id = m_sessions.resolve(credentials.token);

  if (!user_id.has_value()) {
    return std::unexpected(user_id.error());
  }

  if (user_id.value() != credentials.user_id) {
    return std::unexpected(core::CoreError::authorization_failed);
  }

  return user_id.value();
}

Result<PublicationId> DirectTransport::publish(const Credentials &credentials, const PublicationDraft &draft,
                                               const AuthorPublicKey &author_key, const Signature &signature) {
  const auto author_id = authorized(credentials);

  if (!author_id.has_value()) {
    return std::unexpected(author_id.error());
  }

  const auto publication = m_publications.publish(author_id.value(), draft, author_key, signature, m_clock());

  if (!publication.has_value()) {
    return std::unexpected(publication.error());
  }

  return publication->publication_id;
}

Result<Receipt> DirectTransport::buy(const Credentials &credentials, const PublicationId &publication_id,
                                     const DevicePublicKey &device_key) {
  const auto user_id = authorized(credentials);

  if (!user_id.has_value()) {
    return std::unexpected(user_id.error());
  }

  return m_purchases.buy(user_id.value(), publication_id, device_key, m_clock());
}

Result<PurchaseSummaries> DirectTransport::purchases(const Credentials &credentials) {
  const auto user_id = authorized(credentials);

  if (!user_id.has_value()) {
    return std::unexpected(user_id.error());
  }

  return m_purchases.purchases_of(user_id.value());
}

Result<Receipt> DirectTransport::restore_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                                 const DevicePublicKey &device_key) {
  const auto user_id = authorized(credentials);

  if (!user_id.has_value()) {
    return std::unexpected(user_id.error());
  }

  return m_purchases.restore_receipt(user_id.value(), purchase_id, device_key, m_clock());
}

Result<Package> DirectTransport::fetch_package(const Credentials &credentials, const PurchaseId &purchase_id,
                                               const DevicePublicKey &device_key) {
  const auto user_id = authorized(credentials);

  if (!user_id.has_value()) {
    return std::unexpected(user_id.error());
  }

  return m_delivery.fetch_package(user_id.value(), purchase_id, device_key);
}

} // namespace dgds::stubs
