#pragma once

#include <dgds/core/models/content.h>

#include <cstddef>
#include <cstdint>

namespace dgds::core {

inline constexpr std::size_t k_bits_per_byte = 8;
inline constexpr std::size_t k_integer_size = sizeof(std::uint64_t);
inline constexpr std::size_t k_length_size = sizeof(std::uint32_t);

void append_integer(ContentBuffer &data, std::uint64_t value, std::size_t width);
void append_bytes(ContentBuffer &data, const std::uint8_t *bytes, std::size_t size);

class Reader {
public:
  explicit Reader(Content data) : m_data(data) {}

  bool read_integer(std::uint64_t &value, std::size_t width);
  bool read_bytes(std::uint8_t *out, std::size_t size);

  [[nodiscard]] Content rest() const { return m_data.substr(m_offset); }
  [[nodiscard]] std::size_t remaining() const { return m_data.size() - m_offset; }
  [[nodiscard]] bool empty() const { return m_offset == m_data.size(); }

private:
  bool read_byte(std::uint8_t &value);

  Content m_data;
  std::size_t m_offset = 0;
};

} // namespace dgds::core
