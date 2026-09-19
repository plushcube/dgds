#include <dgds/core/envelope/receipt.h>

#include <dgds/core/crypto/sealed_content_codec.h>
#include <dgds/core/envelope/device_wrap.h>

#include <codec/binary.h>

#include <cstddef>
#include <cstdint>
#include <expected>

namespace dgds::core {

ContentBuffer receipt_associated_data(const ReceiptHeader &header) {
  ContentBuffer data;
  data.reserve(1 + k_integer_size + k_user_id_size + 2 * k_integer_size);

  append_integer(data, header.version, 1);
  append_integer(data, header.purchase_id, k_integer_size);
  append_bytes(data, header.user_id.data(), header.user_id.size());
  append_integer(data, static_cast<std::uint64_t>(header.purchased_at), k_integer_size);
  append_integer(data, static_cast<std::uint64_t>(header.issued_at), k_integer_size);

  return data;
}

Result<DeviceEnvelope> wrap_receipt_key(const SymmetricKey &purchase_key, const DevicePublicKey &device_key,
                                        const ReceiptHeader &header) {
  return seal_for_device(purchase_key, device_key, receipt_associated_data(header));
}

Result<SymmetricKey> open_receipt_key(const Receipt &receipt, const DevicePrivateKey &device_key) {
  if (receipt.header.version != k_receipt_version) {
    return std::unexpected(CoreError::receipt_version_unsupported);
  }

  return open_for_device(receipt.wrapped_key, device_key, receipt_associated_data(receipt.header));
}

Result<ContentBuffer> encode_receipt(const Receipt &receipt) {
  const auto sealed = encode_sealed_content(receipt.wrapped_key.wrapped);

  if (!sealed.has_value()) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  ContentBuffer data = receipt_associated_data(receipt.header);

  append_bytes(data, receipt.wrapped_key.ephemeral_key.data(), receipt.wrapped_key.ephemeral_key.size());
  data.append(sealed.value());

  return data;
}

Result<Receipt> decode_receipt(Content data) {
  Reader reader(data);

  ReceiptHeader header{};
  std::uint64_t version = 0;
  std::uint64_t purchase_id = 0;
  std::uint64_t purchased_at = 0;
  std::uint64_t issued_at = 0;

  if (!reader.read_integer(version, 1)) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  if (version != k_receipt_version) {
    return std::unexpected(CoreError::receipt_version_unsupported);
  }

  header.version = static_cast<std::uint8_t>(version);

  if (!reader.read_integer(purchase_id, k_integer_size) ||
      !reader.read_bytes(header.user_id.data(), header.user_id.size()) ||
      !reader.read_integer(purchased_at, k_integer_size) || !reader.read_integer(issued_at, k_integer_size)) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  header.purchase_id = purchase_id;
  header.purchased_at = static_cast<Timestamp>(purchased_at);
  header.issued_at = static_cast<Timestamp>(issued_at);

  DeviceEnvelope envelope{};

  if (!reader.read_bytes(envelope.ephemeral_key.data(), envelope.ephemeral_key.size())) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  auto sealed = decode_sealed_content(reader.rest());

  if (!sealed.has_value()) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  envelope.wrapped = std::move(sealed.value());

  return Receipt{.header = header, .wrapped_key = std::move(envelope)};
}

} // namespace dgds::core
