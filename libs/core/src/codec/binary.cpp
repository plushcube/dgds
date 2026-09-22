#include <dgds/core/codec/binary.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dgds::core {

void append_integer(ContentBuffer &data, std::uint64_t value, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    const std::size_t shift = (width - 1 - index) * k_bits_per_byte;
    data.push_back(static_cast<char>((value >> shift) & 0xFF));
  }
}

void append_bytes(ContentBuffer &data, const std::uint8_t *bytes, std::size_t size) {
  if (size > 0) {
    data.append(reinterpret_cast<const char *>(bytes), size);
  }
}

bool Reader::read_byte(std::uint8_t &value) {
  if (remaining() == 0) {
    return false;
  }

  value = static_cast<std::uint8_t>(m_data[m_offset]);
  ++m_offset;

  return true;
}

bool Reader::read_integer(std::uint64_t &value, std::size_t width) {
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

bool Reader::read_bytes(std::uint8_t *out, std::size_t size) {
  if (size > remaining()) {
    return false;
  }

  if (size > 0) {
    std::memcpy(out, m_data.data() + m_offset, size);
  }

  m_offset += size;

  return true;
}

} // namespace dgds::core
