#include <dgds/core/watermark/mark_channel.h>

#include <dgds/core/watermark/mark_codec.h>

#include <codec/utf8.h>

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

namespace dgds::core {
namespace {

constexpr const char *k_mark_bytes = "\xE2\x80\x8B";
constexpr std::size_t k_mark_bytes_size = 3;
constexpr std::size_t k_max_candidates = 16;

using Window = std::bitset<k_mark_bit_count>;

struct Candidate {
  Mark mark;
  std::size_t count;
};

std::size_t code_point_length(const DecodedCodePoint &decoded) {
  return decoded.code_point == k_unknown_code_point ? 1 : decoded.length;
}

bool is_mark_code_point(const DecodedCodePoint &decoded) { return decoded.code_point == k_mark_code_point; }

bool next_is_mark(Content text, std::size_t offset) {
  return offset < text.size() && is_mark_code_point(decode_utf8(text, offset));
}

MarkBits to_bits(const Window &window) {
  MarkBits bits{};

  for (std::size_t index = 0; index < k_mark_bit_count; ++index) {
    bits[index] = static_cast<std::uint8_t>(window[k_mark_bit_count - 1 - index] ? 1U : 0U);
  }

  return bits;
}

void count_occurrence(std::array<Candidate, k_max_candidates> &candidates, std::size_t &count, bool &overflowed,
                      const Mark &mark) {
  for (std::size_t index = 0; index < count; ++index) {
    if (candidates[index].mark == mark) {
      ++candidates[index].count;
      return;
    }
  }

  if (count == k_max_candidates) {
    overflowed = true;
    return;
  }

  candidates[count] = Candidate{.mark = mark, .count = 1};
  ++count;
}

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
  std::array<Candidate, k_max_candidates> candidates{};
  std::size_t candidate_count = 0;
  bool overflowed = false;
  bool mark_seen = false;
  bool window_seen = false;
  bool valid_seen = false;
  Window window;
  std::size_t filled = 0;

  for (std::size_t offset = 0; offset < text.size();) {
    const DecodedCodePoint decoded = decode_utf8(text, offset);
    const std::size_t length = code_point_length(decoded);

    offset += length;

    if (is_mark_code_point(decoded)) {
      mark_seen = true;
      continue;
    }

    window <<= 1;

    if (next_is_mark(text, offset)) {
      window.set(0);
    }

    if (++filled < k_mark_bit_count) {
      continue;
    }

    window_seen = true;

    const auto mark = decode_mark(to_bits(window));

    if (mark.has_value()) {
      valid_seen = true;
      count_occurrence(candidates, candidate_count, overflowed, mark.value());
    }
  }

  const Candidate *confident = nullptr;

  for (std::size_t index = 0; index < candidate_count; ++index) {
    if (candidates[index].count < k_mark_confidence_threshold) {
      continue;
    }

    if (confident != nullptr) {
      return std::unexpected(CoreError::mark_not_confident);
    }

    confident = &candidates[index];
  }

  if (confident != nullptr) {
    return confident->mark;
  }

  if (!mark_seen) {
    return std::unexpected(CoreError::mark_not_found);
  }

  if (overflowed || valid_seen || !window_seen) {
    return std::unexpected(CoreError::mark_not_confident);
  }

  return std::unexpected(CoreError::mark_malformed);
}

} // namespace dgds::core
