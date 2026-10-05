#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/mark.h>
#include <dgds/core/watermark/mark_channel.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

using dgds::core::CanonicalForm;
using dgds::core::Content;
using dgds::core::content_identity;
using dgds::core::CoreError;
using dgds::core::embed_mark;
using dgds::core::has_mark_channel;
using dgds::core::k_mark_bit_count;
using dgds::core::k_mark_version;
using dgds::core::Mark;
using dgds::core::MarkBits;
using dgds::core::read_marks;

constexpr std::string_view k_mark_bytes = "\xE2\x80\x8B";

constexpr std::size_t k_bits_per_byte = 8;
constexpr std::size_t k_legacy_payload_size = 9;
constexpr std::size_t k_legacy_checksum_bit_count = 16;
constexpr std::uint8_t k_legacy_version = 1;
constexpr std::uint16_t k_crc_polynomial = 0x1021;
constexpr std::uint16_t k_crc_initial = 0xFFFF;
constexpr std::uint16_t k_crc_high_bit = 0x8000;

using LegacyPayload = std::array<std::uint8_t, k_legacy_payload_size>;

Mark make_mark(std::uint64_t purchase_id) {
  Mark mark{.purchase_id = purchase_id, .code = {}, .version = k_mark_version};

  for (std::size_t index = 0; index < mark.code.size(); ++index) {
    mark.code[index] = static_cast<std::uint8_t>(purchase_id + index);
  }

  return mark;
}

bool read_contains(const std::vector<Mark> &marks, const Mark &expected) {
  return std::find(marks.begin(), marks.end(), expected) != marks.end();
}

std::string long_text(std::size_t lines) {
  std::string text;

  for (std::size_t index = 0; index < lines; ++index) {
    text += "line " + std::to_string(index) + " of the publication\n";
  }

  return text;
}

std::size_t offset_of_significant(Content text, std::size_t count) {
  std::size_t significant = 0;

  for (std::size_t offset = 0; offset < text.size();) {
    if (text.compare(offset, k_mark_bytes.size(), k_mark_bytes) == 0) {
      offset += k_mark_bytes.size();
      continue;
    }

    if (significant == count) {
      return offset;
    }

    ++significant;
    ++offset;
  }

  return text.size();
}

std::string embed_bits(Content text, const MarkBits &bits) {
  std::string marked;
  marked.reserve(text.size() + bits.size());
  std::size_t significant = 0;

  for (const char symbol : text) {
    marked.push_back(symbol);

    if (bits[significant % k_mark_bit_count] == 1) {
      marked.append(k_mark_bytes);
    }

    ++significant;
  }

  return marked;
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

MarkBits legacy_bits(std::uint64_t purchase_id) {
  LegacyPayload payload{};
  payload[0] = k_legacy_version;

  for (std::size_t index = 0; index < sizeof(purchase_id); ++index) {
    payload[index + 1] =
        static_cast<std::uint8_t>(purchase_id >> ((sizeof(purchase_id) - 1 - index) * k_bits_per_byte));
  }

  const std::uint16_t checksum = crc16(payload);
  MarkBits bits{};

  for (std::size_t index = 0; index < payload.size(); ++index) {
    for (std::size_t bit = 0; bit < k_bits_per_byte; ++bit) {
      bits[index * k_bits_per_byte + bit] =
          static_cast<std::uint8_t>((payload[index] >> (k_bits_per_byte - 1 - bit)) & 1U);
    }
  }

  for (std::size_t bit = 0; bit < k_legacy_checksum_bit_count; ++bit) {
    const std::size_t offset = k_legacy_payload_size * k_bits_per_byte + bit;
    bits[offset] = static_cast<std::uint8_t>((checksum >> (k_legacy_checksum_bit_count - 1 - bit)) & 1U);
  }

  return bits;
}

TEST(MarkChannel, EmbedsDeterministically) {
  const std::string text = long_text(10);
  const Mark mark = make_mark(4242);

  const auto first = embed_mark(text, mark);
  const auto second = embed_mark(text, mark);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first.value(), second.value());
  EXPECT_GT(first->size(), text.size());
}

TEST(MarkChannel, ReadsMarkFromAnyOccurrence) {
  const std::string text = long_text(30);
  const Mark mark = make_mark(777);

  const auto marked = embed_mark(text, mark);
  ASSERT_TRUE(marked.has_value());

  const auto whole = read_marks(marked.value());
  ASSERT_TRUE(whole.has_value());
  EXPECT_TRUE(read_contains(whole.value(), mark));

  for (std::size_t block = 1; block < 4; ++block) {
    const std::size_t offset = offset_of_significant(marked.value(), block * k_mark_bit_count);
    ASSERT_LT(offset, marked->size());

    const auto occurrence = read_marks(Content(marked->data() + offset, marked->size() - offset));

    ASSERT_TRUE(occurrence.has_value()) << block;
    EXPECT_TRUE(read_contains(occurrence.value(), mark)) << block;
  }
}

TEST(MarkChannel, KeepsContentAndIdentityAfterCanonicalization) {
  const std::string text =
      "публикация с кириллицей и эмодзи \xF0\x9F\x93\x84 внутри текста для проверки канала метки. " +
      std::string("повторяем строку, чтобы в текст поместился целый блок метки. ");
  const Mark mark = make_mark(99);

  const auto marked = embed_mark(text, mark);
  ASSERT_TRUE(marked.has_value());
  EXPECT_NE(marked.value(), text);

  const CanonicalForm canonical = dgds::core::canonical_form(marked.value());
  EXPECT_EQ(canonical, text);

  const auto original_identity = content_identity(text);
  const auto marked_identity = content_identity(marked.value());

  ASSERT_TRUE(original_identity.has_value());
  ASSERT_TRUE(marked_identity.has_value());
  EXPECT_EQ(marked_identity.value(), original_identity.value());
}

TEST(MarkChannel, ReportsMissingChannel) {
  const std::string short_text(k_mark_bit_count - 1, 'x');
  const std::string enough_text(k_mark_bit_count, 'x');

  EXPECT_FALSE(has_mark_channel(short_text));
  EXPECT_FALSE(embed_mark(short_text, make_mark(1)).has_value());

  EXPECT_TRUE(has_mark_channel(enough_text));
  EXPECT_TRUE(embed_mark(enough_text, make_mark(1)).has_value());
}

TEST(MarkChannel, RefusesTextOutsideChannelDomain) {
  const std::string enough = long_text(30);

  ASSERT_TRUE(has_mark_channel(enough));

  EXPECT_FALSE(has_mark_channel(std::string("\xC0\x80") + enough));
  EXPECT_FALSE(has_mark_channel(std::string("\xE0\x80\x80") + enough));
  EXPECT_FALSE(has_mark_channel(std::string("\xF0\x80\x80\x80") + enough));
  EXPECT_FALSE(has_mark_channel(std::string("\xED\xA0\x80") + enough));
  EXPECT_FALSE(has_mark_channel(std::string("\xF5\x80\x80\x80") + enough));
  EXPECT_FALSE(has_mark_channel(std::string("\xE2\x80") + enough));
  EXPECT_FALSE(has_mark_channel(std::string("\x80") + enough));
  EXPECT_FALSE(embed_mark(std::string("\xED\xA0\x80") + enough, make_mark(3)).has_value());
}

TEST(MarkChannel, KeepsChannelForValidTextOutsideBmp) {
  const std::string enough = long_text(30);
  const std::string with_emoji = enough + "\xF0\x9F\x93\x84";
  const std::string with_cyrillic = enough + "текст";

  EXPECT_TRUE(has_mark_channel(with_emoji));
  EXPECT_TRUE(has_mark_channel(with_cyrillic));
  EXPECT_TRUE(embed_mark(with_emoji, make_mark(7)).has_value());
}

TEST(MarkChannel, ReadsMarkFromUnalignedExcerpt) {
  const std::string text = long_text(30);
  const Mark mark = make_mark(31337);

  const auto marked = embed_mark(text, mark);
  ASSERT_TRUE(marked.has_value());

  const std::size_t offset = offset_of_significant(marked.value(), 37);
  const auto excerpt = read_marks(Content(marked->data() + offset, marked->size() - offset));

  ASSERT_TRUE(excerpt.has_value());
  EXPECT_TRUE(read_contains(excerpt.value(), mark));
}

TEST(MarkChannel, SurvivesPartialTruncation) {
  const std::string text = long_text(30);
  const Mark mark = make_mark(5);

  const auto marked = embed_mark(text, mark);
  ASSERT_TRUE(marked.has_value());

  const std::size_t cut = offset_of_significant(marked.value(), k_mark_bit_count * 3);
  const auto truncated = read_marks(Content(marked->data(), cut));

  ASSERT_TRUE(truncated.has_value());
  EXPECT_TRUE(read_contains(truncated.value(), mark));
}

TEST(MarkChannel, ToleratesDamageWhileOccurrencesRemain) {
  const std::string text = long_text(30);
  const Mark mark = make_mark(777);

  const auto marked = embed_mark(text, mark);
  ASSERT_TRUE(marked.has_value());

  std::string damaged = marked.value();
  const std::size_t last = damaged.rfind(k_mark_bytes);
  ASSERT_NE(last, std::string::npos);
  damaged.erase(last, k_mark_bytes.size());

  const auto read = read_marks(damaged);

  ASSERT_TRUE(read.has_value());
  EXPECT_TRUE(read_contains(read.value(), mark));
}

TEST(MarkChannel, RequiresEnoughOccurrences) {
  const std::string text(k_mark_bit_count, 'x');

  const auto marked = embed_mark(text, make_mark(11));
  ASSERT_TRUE(marked.has_value());

  const auto read = read_marks(marked.value());

  ASSERT_FALSE(read.has_value());
  EXPECT_EQ(read.error(), CoreError::mark_not_confident);
}

TEST(MarkChannel, ReportsAbsentMark) {
  const std::string text = long_text(20);

  const auto read = read_marks(text);

  ASSERT_FALSE(read.has_value());
  EXPECT_EQ(read.error(), CoreError::mark_not_found);
}

TEST(MarkChannel, ReportsEveryConflictingOccurrence) {
  const std::string text = long_text(60);
  const Mark first_mark = make_mark(1);
  const Mark second_mark = make_mark(2);

  const auto first = embed_mark(text, first_mark);
  const auto second = embed_mark(text, second_mark);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  const std::size_t cut_first = offset_of_significant(first.value(), k_mark_bit_count * 4);
  const std::size_t cut_second = offset_of_significant(second.value(), k_mark_bit_count * 4);

  const std::string spliced = first->substr(0, cut_first) + second->substr(cut_second);

  const auto read = read_marks(spliced);

  ASSERT_TRUE(read.has_value());
  EXPECT_TRUE(read_contains(read.value(), first_mark));
  EXPECT_TRUE(read_contains(read.value(), second_mark));
}

TEST(MarkChannel, RejectsLegacyMarkVersion) {
  const std::string marked = embed_bits(long_text(30), legacy_bits(5));

  const auto read = read_marks(marked);

  ASSERT_FALSE(read.has_value());
  EXPECT_EQ(read.error(), CoreError::mark_version_unsupported);
}

} // namespace
