#include <dgds/client/client.h>

#include <dgds/core/envelope/package.h>
#include <dgds/core/envelope/receipt.h>

#include <expected>

namespace dgds::client {

Result<Receipt> ApiClient::buy(const Credentials &credentials, const PublicationId &publication_id) {
  const auto device_key = m_device_key.public_key();

  if (!device_key.has_value()) {
    return std::unexpected(device_key.error());
  }

  const auto receipt = m_transport.buy(credentials, publication_id, device_key.value());

  if (!receipt.has_value()) {
    return std::unexpected(receipt.error());
  }

  const auto stored = save_receipt(receipt.value());

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  return receipt.value();
}

Result<Receipt> ApiClient::stored_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                          const DevicePublicKey &device_key) {
  const auto stored = m_receipts.load(purchase_id);

  if (stored.has_value()) {
    return core::decode_receipt(stored.value());
  }

  if (stored.error() != core::CoreError::receipt_not_found) {
    return std::unexpected(stored.error());
  }

  const auto restored = m_transport.restore_receipt(credentials, purchase_id, device_key);

  if (!restored.has_value()) {
    return std::unexpected(restored.error());
  }

  const auto saved = save_receipt(restored.value());

  if (!saved.has_value()) {
    return std::unexpected(saved.error());
  }

  return restored.value();
}

Result<SecureBuffer> ApiClient::fetch_content(const Credentials &credentials, const PurchaseId &purchase_id) {
  const auto device_key = m_device_key.public_key();

  if (!device_key.has_value()) {
    return std::unexpected(device_key.error());
  }

  const auto receipt = stored_receipt(credentials, purchase_id, device_key.value());

  if (!receipt.has_value()) {
    return std::unexpected(receipt.error());
  }

  const auto receipt_key = m_device_key.open_receipt_key(receipt.value());

  if (!receipt_key.has_value()) {
    return std::unexpected(receipt_key.error());
  }

  const auto package = m_transport.fetch_package(credentials, purchase_id, device_key.value());

  if (!package.has_value()) {
    return std::unexpected(package.error());
  }

  return core::open_package(package.value(), receipt_key.value());
}

Result<void> ApiClient::save_receipt(const Receipt &receipt) {
  const auto encoded = core::encode_receipt(receipt);

  if (!encoded.has_value()) {
    return std::unexpected(encoded.error());
  }

  return m_receipts.save(receipt.header.purchase_id, encoded.value());
}

} // namespace dgds::client
