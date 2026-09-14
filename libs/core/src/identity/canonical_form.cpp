#include <dgds/core/identity/canonical_form.h>

#include <cstddef>
#include <cstdint>

namespace dgds::core {
namespace {

using CodePoint = char32_t;
using ByteCount = std::size_t;

constexpr CodePoint k_unknown = 0xFFFFFFFF;

constexpr CodePoint k_zero_width_space = 0x200B;
constexpr CodePoint k_variation_selector_first = 0xFE00;
constexpr CodePoint k_variation_selector_last = 0xFE0F;
constexpr CodePoint k_supplementary_variation_selector_first = 0xE0100;
constexpr CodePoint k_supplementary_variation_selector_last = 0xE01EF;

struct Decoded {
  CodePoint code_point;
  ByteCount length;
};

bool is_ascii_alphanumeric(CodePoint code_point) {
  return (code_point >= U'a' && code_point <= U'z') || (code_point >= U'A' && code_point <= U'Z') ||
         (code_point >= U'0' && code_point <= U'9');
}

bool is_variation_selector(CodePoint code_point) {
  return (code_point >= k_variation_selector_first && code_point <= k_variation_selector_last) ||
         (code_point >= k_supplementary_variation_selector_first &&
          code_point <= k_supplementary_variation_selector_last);
}

bool is_mark_channel(CodePoint code_point, CodePoint previous) {
  return code_point == k_zero_width_space || (is_variation_selector(code_point) && is_ascii_alphanumeric(previous));
}

Decoded decode(Content content, ByteCount offset) {
  const auto lead = static_cast<std::uint8_t>(content[offset]);

  CodePoint code_point = 0;
  ByteCount length = 1;

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
    return {k_unknown, 1};
  }

  if (offset + length > content.size()) {
    return {k_unknown, 1};
  }

  for (ByteCount index = 1; index < length; ++index) {
    const auto byte = static_cast<std::uint8_t>(content[offset + index]);

    if ((byte & 0xC0) != 0x80) {
      return {k_unknown, 1};
    }

    code_point = (code_point << 6) | (byte & 0x3F);
  }

  return {code_point, length};
}

} // namespace

CanonicalForm canonical_form(Content content) {
  CanonicalForm result;
  result.reserve(content.size());

  CodePoint previous = k_unknown;

  for (ByteCount offset = 0; offset < content.size();) {
    const Decoded decoded = decode(content, offset);

    if (decoded.code_point == k_unknown) {
      result.push_back(content[offset]);
      previous = k_unknown;
      ++offset;
      continue;
    }

    if (!is_mark_channel(decoded.code_point, previous)) {
      result.append(content.substr(offset, decoded.length));
      previous = decoded.code_point;
    }

    offset += decoded.length;
  }

  return result;
}

} // namespace dgds::core
