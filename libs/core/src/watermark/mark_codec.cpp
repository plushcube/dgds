#include <dgds/core/watermark/mark_codec.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace dgds::core {
namespace {

constexpr std::size_t k_bits_per_byte = 8;
constexpr std::size_t k_version_bit_count = 8;
constexpr std::size_t k_id_bit_count = 64;
constexpr std::size_t k_id_bit_offset = k_version_bit_count;
constexpr std::size_t k_code_bit_offset = k_id_bit_offset + k_id_bit_count;

constexpr std::uint8_t k_legacy_version = 1;
constexpr std::size_t k_legacy_payload_size = 9;
constexpr std::size_t k_legacy_payload_bit_count = k_legacy_payload_size * k_bits_per_byte;
constexpr std::size_t k_legacy_checksum_bit_count = 16;

constexpr std::uint16_t k_crc_polynomial = 0x1021;
constexpr std::uint16_t k_crc_initial = 0xFFFF;
constexpr std::uint16_t k_crc_high_bit = 0x8000;

using LegacyPayload = std::array<std::uint8_t, k_legacy_payload_size>;

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

std::uint16_t crc16(const LegacyPayload &payload) {
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

LegacyPayload to_legacy_payload(const MarkBits &bits) {
  LegacyPayload payload{};

  for (std::size_t index = 0; index < payload.size(); ++index) {
    payload[index] = static_cast<std::uint8_t>(read_bits(bits, index * k_bits_per_byte, k_bits_per_byte));
  }

  return payload;
}

} // namespace

MarkBits encode_mark(const Mark &mark) {
  MarkBits bits{};
  write_bits(bits, 0, mark.version, k_version_bit_count);
  write_bits(bits, k_id_bit_offset, mark.purchase_id, k_id_bit_count);

  for (std::size_t index = 0; index < k_mark_code_size; ++index) {
    write_bits(bits, k_code_bit_offset + index * k_bits_per_byte, mark.code[index], k_bits_per_byte);
  }

  return bits;
}

Mark decode_mark(const MarkBits &bits) {
  Mark mark{.purchase_id = read_bits(bits, k_id_bit_offset, k_id_bit_count),
            .code = {},
            .version = static_cast<std::uint8_t>(read_bits(bits, 0, k_version_bit_count))};

  for (std::size_t index = 0; index < k_mark_code_size; ++index) {
    mark.code[index] =
        static_cast<std::uint8_t>(read_bits(bits, k_code_bit_offset + index * k_bits_per_byte, k_bits_per_byte));
  }

  return mark;
}

bool is_legacy_mark(const MarkBits &bits) {
  if (read_bits(bits, 0, k_version_bit_count) != k_legacy_version) {
    return false;
  }

  return read_bits(bits, k_legacy_payload_bit_count, k_legacy_checksum_bit_count) == crc16(to_legacy_payload(bits));
}

} // namespace dgds::core
