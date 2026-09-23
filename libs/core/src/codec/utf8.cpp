#include <codec/utf8.h>

#include <cstddef>
#include <cstdint>

namespace dgds::core {

DecodedCodePoint decode_utf8(Content content, std::size_t offset) {
  const auto lead = static_cast<std::uint8_t>(content[offset]);

  char32_t code_point = 0;
  std::size_t length = 1;

  if (lead < 0x80) {
    code_point = lead;
  } else if ((lead & 0xE0) == 0xC0) {
    length = 2;
    code_point = lead & 0x1F;
  } else if ((lead & 0xF0) == 0xE0) {
    length = 3;
    code_point = lead & 0x0F;
  } else if ((lead & 0xF8) == 0xF0) {
    length = 4;
    code_point = lead & 0x07;
  } else {
    return {k_unknown_code_point, 1};
  }

  if (offset + length > content.size()) {
    return {k_unknown_code_point, 1};
  }

  for (std::size_t index = 1; index < length; ++index) {
    const auto byte = static_cast<std::uint8_t>(content[offset + index]);

    if ((byte & 0xC0) != 0x80) {
      return {k_unknown_code_point, 1};
    }

    code_point = (code_point << 6) | (byte & 0x3F);
  }

  return {code_point, length};
}

} // namespace dgds::core
