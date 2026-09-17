#include <dgds/core/envelope/receipt.h>

#include <dgds/core/envelope/device_wrap.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>

namespace dgds::core {
namespace {

constexpr std::size_t k_bits_per_byte = 8;
constexpr std::size_t k_integer_size = sizeof(std::uint64_t);
constexpr std::size_t k_length_size = sizeof(std::uint32_t);
constexpr std::uint64_t k_max_length = std::numeric_limits<std::uint32_t>::max();

void append_integer(ContentBuffer &data, std::uint64_t value, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    const std::size_t shift = (width - 1 - index) * k_bits_per_byte;
    data.push_back(static_cast<char>((value >> shift) & 0xFF));
  }
}

void append_bytes(ContentBuffer &data, const std::uint8_t *bytes, std::size_t size) {
  data.append(reinterpret_cast<const char *>(bytes), size);
}

class Reader {
public:
  explicit Reader(Content data) : m_data(data) {}

  bool read_integer(std::uint64_t &value, std::size_t width) {
    value = 0;

    for (std::size_t index = 0; index < width; ++index) {
      std::uint8_t byte = 0;

      if (!read_byte(byte)) {
        return false;
      }

      value = (value << k_bits_per_byte) | byte;
    }

    return true;
  }

  bool read_bytes(std::uint8_t *out, std::size_t size) {
    if (size > remaining()) {
      return false;
    }

    if (size > 0) {
      std::memcpy(out, m_data.data() + m_offset, size);
    }

    m_offset += size;

    return true;
  }

  [[nodiscard]] std::size_t remaining() const { return m_data.size() - m_offset; }
  [[nodiscard]] bool empty() const { return m_offset == m_data.size(); }

private:
  bool read_byte(std::uint8_t &value) {
    if (remaining() == 0) {
      return false;
    }

    value = static_cast<std::uint8_t>(m_data[m_offset]);
    ++m_offset;

    return true;
  }

  Content m_data;
  std::size_t m_offset = 0;
};

} // namespace

ContentBuffer receipt_associated_data(const ReceiptHeader &header) {
  ContentBuffer data;
  data.reserve(1 + k_integer_size + k_user_id_size + 2 * k_integer_size);

  data.push_back(static_cast<char>(header.version));
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
  const SealedContent &sealed = receipt.wrapped_key.wrapped;

  if (sealed.ciphertext.size() > k_max_length) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  ContentBuffer data = receipt_associated_data(receipt.header);

  append_bytes(data, receipt.wrapped_key.ephemeral_key.data(), receipt.wrapped_key.ephemeral_key.size());
  data.push_back(static_cast<char>(sealed.algorithm));
  append_bytes(data, sealed.nonce.data(), sealed.nonce.size());
  append_integer(data, sealed.ciphertext.size(), k_length_size);
  append_bytes(data, sealed.ciphertext.data(), sealed.ciphertext.size());
  append_bytes(data, sealed.tag.data(), sealed.tag.size());

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
  std::uint64_t algorithm = 0;
  std::uint64_t length = 0;

  if (!reader.read_bytes(envelope.ephemeral_key.data(), envelope.ephemeral_key.size()) ||
      !reader.read_integer(algorithm, 1) ||
      !reader.read_bytes(envelope.wrapped.nonce.data(), envelope.wrapped.nonce.size()) ||
      !reader.read_integer(length, k_length_size)) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  if (length > reader.remaining()) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  if (length > k_max_length) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  envelope.wrapped.algorithm = static_cast<AeadAlgorithm>(algorithm);
  envelope.wrapped.ciphertext.resize(static_cast<std::size_t>(length));

  if (!reader.read_bytes(envelope.wrapped.ciphertext.data(), envelope.wrapped.ciphertext.size()) ||
      !reader.read_bytes(envelope.wrapped.tag.data(), envelope.wrapped.tag.size()) || !reader.empty()) {
    return std::unexpected(CoreError::receipt_malformed);
  }

  return Receipt{.header = header, .wrapped_key = envelope};
}

} // namespace dgds::core
