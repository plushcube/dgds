#include <dgds/core/watermark/mark_channel.h>

#include <dgds/core/watermark/mark_codec.h>

#include <codec/utf8.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace dgds::core {
namespace {

constexpr const char *k_mark_bytes = "\xE2\x80\x8B";
constexpr std::size_t k_mark_bytes_size = 3;

std::size_t code_point_length(const DecodedCodePoint &decoded) {
  return decoded.code_point == k_unknown_code_point ? 1 : decoded.length;
}

bool is_mark_code_point(const DecodedCodePoint &decoded) { return decoded.code_point == k_mark_code_point; }

} // namespace

bool has_mark_channel(Content text) {
  std::size_t significant = 0;

  for (std::size_t offset = 0; offset < text.size();) {
    const DecodedCodePoint decoded = decode_utf8(text, offset);

    if (!is_mark_code_point(decoded)) {
      ++significant;
    }

    offset += code_point_length(decoded);
  }

  return significant >= k_mark_bit_count;
}

std::optional<ContentBuffer> embed_mark(Content text, const Mark &mark) {
  if (!has_mark_channel(text)) {
    return std::nullopt;
  }

  const MarkBits bits = encode_mark(mark);

  ContentBuffer marked;
  marked.reserve(text.size() + text.size() / 2);

  std::size_t significant = 0;

  for (std::size_t offset = 0; offset < text.size();) {
    const DecodedCodePoint decoded = decode_utf8(text, offset);
    const std::size_t length = code_point_length(decoded);

    marked.append(text.substr(offset, length));

    if (!is_mark_code_point(decoded)) {
      if (bits[significant % k_mark_bit_count] == 1) {
        marked.append(k_mark_bytes, k_mark_bytes_size);
      }

      ++significant;
    }

    offset += length;
  }

  return marked;
}

Result<Mark> read_mark(Content text) {
  MarkBits bits{};
  std::size_t filled = 0;
  std::size_t offset = 0;
  std::optional<CoreError> first_failure;

  while (offset < text.size()) {
    const DecodedCodePoint decoded = decode_utf8(text, offset);
    const std::size_t length = code_point_length(decoded);

    if (is_mark_code_point(decoded)) {
      offset += length;
      continue;
    }

    offset += length;

    if (offset < text.size()) {
      const DecodedCodePoint next = decode_utf8(text, offset);

      if (is_mark_code_point(next)) {
        bits[filled] = 1;
      }
    }

    ++filled;

    if (filled == k_mark_bit_count) {
      const auto mark = decode_mark(bits);

      if (mark.has_value()) {
        return mark;
      }

      if (!first_failure.has_value()) {
        first_failure = mark.error();
      }

      bits.fill(0);
      filled = 0;
    }
  }

  return std::unexpected(first_failure.value_or(CoreError::mark_malformed));
}

} // namespace dgds::core
