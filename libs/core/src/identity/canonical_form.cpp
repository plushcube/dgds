#include <dgds/core/identity/canonical_form.h>

#include <dgds/core/models/mark.h>

#include <codec/utf8.h>

#include <cstddef>
#include <cstdint>

namespace dgds::core {
namespace {

using CodePoint = char32_t;

constexpr CodePoint k_variation_selector_first = 0xFE00;
constexpr CodePoint k_variation_selector_last = 0xFE0F;
constexpr CodePoint k_supplementary_variation_selector_first = 0xE0100;
constexpr CodePoint k_supplementary_variation_selector_last = 0xE01EF;

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
  return code_point == k_mark_code_point || (is_variation_selector(code_point) && is_ascii_alphanumeric(previous));
}

} // namespace

CanonicalForm canonical_form(Content content) {
  CanonicalForm result;
  result.reserve(content.size());

  CodePoint previous = k_unknown_code_point;

  for (std::size_t offset = 0; offset < content.size();) {
    const DecodedCodePoint decoded = decode_utf8(content, offset);

    if (decoded.code_point == k_unknown_code_point) {
      result.push_back(content[offset]);
      previous = k_unknown_code_point;
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
