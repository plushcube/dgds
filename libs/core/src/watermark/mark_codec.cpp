#include <dgds/core/watermark/mark_codec.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace dgds::core {
namespace {

using Payload = std::array<std::uint8_t, 9>;

constexpr std::size_t k_bits_per_byte = 8;
constexpr std::size_t k_checksum_bit_count = 16;
constexpr std::size_t k_id_bit_count = 64;
constexpr std::size_t k_id_bit_offset = k_bits_per_byte;
constexpr std::size_t k_checksum_bit_offset = k_mark_bit_count - k_checksum_bit_count;

constexpr std::uint16_t k_crc_polynomial = 0x1021;
constexpr std::uint16_t k_crc_initial = 0xFFFF;
constexpr std::uint16_t k_crc_high_bit = 0x8000;

void write_bits(MarkBits &bits, std::size_t offset, std::uint64_t value, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    const std::size_t shift = width - 1 - index;
    bits[offset + index] = static_cast<std::uint8_t>((value >> shift) & 1U);
  }
}

std::uint64_t read_bits(const MarkBits &bits, std::size_t offset, std::size_t width) {
  std::uint64_t value = 0;

  for (std::size_t index = 0; index < width; ++index) {
    value = (value << 1) | bits[offset + index];
  }

  return value;
}

Payload to_payload(const Mark &mark) {
  Payload payload{};
  payload[0] = mark.version;

  for (std::size_t index = 0; index < sizeof(PurchaseId); ++index) {
    const std::size_t shift = (sizeof(PurchaseId) - 1 - index) * k_bits_per_byte;
    payload[index + 1] = static_cast<std::uint8_t>(mark.purchase_id >> shift);
  }

  return payload;
}

std::uint16_t crc16(const Payload &payload) {
  std::uint16_t crc = k_crc_initial;

  for (const std::uint8_t byte : payload) {
    crc ^= static_cast<std::uint16_t>(byte) << k_bits_per_byte;

    for (std::size_t bit = 0; bit < k_bits_per_byte; ++bit) {
      const bool carry = (crc & k_crc_high_bit) != 0;
      crc = static_cast<std::uint16_t>(crc << 1);

      if (carry) {
        crc ^= k_crc_polynomial;
      }
    }
  }

  return crc;
}

} // namespace

MarkBits encode_mark(const Mark &mark) {
  MarkBits bits{};
  const Payload payload = to_payload(mark);

  for (std::size_t index = 0; index < payload.size(); ++index) {
    write_bits(bits, index * k_bits_per_byte, payload[index], k_bits_per_byte);
  }

  write_bits(bits, k_checksum_bit_offset, crc16(payload), k_checksum_bit_count);

  return bits;
}

Result<Mark> decode_mark(const MarkBits &bits) {
  for (const std::uint8_t bit : bits) {
    if (bit > 1) {
      return std::unexpected(CoreError::mark_malformed);
    }
  }

  Payload payload{};

  for (std::size_t index = 0; index < payload.size(); ++index) {
    payload[index] = static_cast<std::uint8_t>(read_bits(bits, index * k_bits_per_byte, k_bits_per_byte));
  }

  const auto checksum = static_cast<std::uint16_t>(read_bits(bits, k_checksum_bit_offset, k_checksum_bit_count));

  if (checksum != crc16(payload)) {
    return std::unexpected(CoreError::mark_checksum_mismatch);
  }

  if (payload[0] != k_mark_version) {
    return std::unexpected(CoreError::mark_version_unsupported);
  }

  return Mark{.purchase_id = read_bits(bits, k_id_bit_offset, k_id_bit_count), .version = payload[0]};
}

} // namespace dgds::core
