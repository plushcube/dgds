#include <dgds/core/envelope/receipt.h>

#include <dgds/core/envelope/device_wrap.h>

#include <cstddef>
#include <cstdint>
#include <expected>

namespace dgds::core {
namespace {

constexpr std::size_t k_bits_per_byte = 8;
constexpr std::size_t k_integer_size = sizeof(std::uint64_t);

void append_integer(ContentBuffer &data, std::uint64_t value) {
  for (std::size_t index = 0; index < k_integer_size; ++index) {
    const std::size_t shift = (k_integer_size - 1 - index) * k_bits_per_byte;
    data.push_back(static_cast<char>((value >> shift) & 0xFF));
  }
}

void append_bytes(ContentBuffer &data, const std::uint8_t *bytes, std::size_t size) {
  data.append(reinterpret_cast<const char *>(bytes), size);
}

} // namespace

ContentBuffer receipt_associated_data(const ReceiptHeader &header) {
  ContentBuffer data;
  data.reserve(1 + k_integer_size + k_user_id_size + 2 * k_integer_size);

  data.push_back(static_cast<char>(header.version));
  append_integer(data, header.purchase_id);
  append_bytes(data, header.user_id.data(), header.user_id.size());
  append_integer(data, static_cast<std::uint64_t>(header.purchased_at));
  append_integer(data, static_cast<std::uint64_t>(header.issued_at));

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

} // namespace dgds::core
