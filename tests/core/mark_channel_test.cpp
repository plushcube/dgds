#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/mark.h>
#include <dgds/core/watermark/mark_channel.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

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
using dgds::core::read_mark;

constexpr std::string_view k_mark_bytes = "\xE2\x80\x8B";

Mark make_mark(std::uint64_t purchase_id) { return Mark{.purchase_id = purchase_id, .version = k_mark_version}; }

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

  const auto whole = read_mark(marked.value());
  ASSERT_TRUE(whole.has_value());
  EXPECT_EQ(whole.value(), mark);

  for (std::size_t block = 1; block < 4; ++block) {
    const std::size_t offset = offset_of_significant(marked.value(), block * k_mark_bit_count);
    ASSERT_LT(offset, marked->size());

    const auto occurrence = read_mark(Content(marked->data() + offset, marked->size() - offset));

    ASSERT_TRUE(occurrence.has_value()) << block;
    EXPECT_EQ(occurrence.value(), mark) << block;
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

TEST(MarkChannel, ReadsMarkFromUnalignedExcerpt) {
  const std::string text = long_text(30);
  const Mark mark = make_mark(31337);

  const auto marked = embed_mark(text, mark);
  ASSERT_TRUE(marked.has_value());

  const std::size_t offset = offset_of_significant(marked.value(), 37);
  const auto excerpt = read_mark(Content(marked->data() + offset, marked->size() - offset));

  ASSERT_TRUE(excerpt.has_value());
  EXPECT_EQ(excerpt.value(), mark);
}

TEST(MarkChannel, SurvivesPartialTruncation) {
  const std::string text = long_text(30);
  const Mark mark = make_mark(5);

  const auto marked = embed_mark(text, mark);
  ASSERT_TRUE(marked.has_value());

  const std::size_t cut = offset_of_significant(marked.value(), k_mark_bit_count * 3);
  const auto truncated = read_mark(Content(marked->data(), cut));

  ASSERT_TRUE(truncated.has_value());
  EXPECT_EQ(truncated.value(), mark);
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

  const auto read = read_mark(damaged);

  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read.value(), mark);
}

TEST(MarkChannel, RequiresEnoughOccurrences) {
  const std::string text(k_mark_bit_count, 'x');

  const auto marked = embed_mark(text, make_mark(11));
  ASSERT_TRUE(marked.has_value());

  const auto read = read_mark(marked.value());

  ASSERT_FALSE(read.has_value());
  EXPECT_EQ(read.error(), CoreError::mark_not_confident);
}

TEST(MarkChannel, RejectsDamagedMark) {
  const std::string text(k_mark_bit_count + 8, 'x');

  const auto marked = embed_mark(text, make_mark(12));
  ASSERT_TRUE(marked.has_value());

  std::string damaged = marked.value();
  const std::size_t first = damaged.find(k_mark_bytes);
  ASSERT_NE(first, std::string::npos);
  damaged.erase(first, k_mark_bytes.size());

  const auto read = read_mark(damaged);

  ASSERT_FALSE(read.has_value());
  EXPECT_EQ(read.error(), CoreError::mark_malformed);
}

TEST(MarkChannel, ReportsAbsentMark) {
  const std::string text = long_text(20);

  const auto read = read_mark(text);

  ASSERT_FALSE(read.has_value());
  EXPECT_EQ(read.error(), CoreError::mark_not_found);
}

TEST(MarkChannel, RejectsConflictingOccurrences) {
  const std::string text = long_text(30);

  const auto first = embed_mark(text, make_mark(1));
  const auto second = embed_mark(text, make_mark(2));
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  const std::size_t cut_first = offset_of_significant(first.value(), k_mark_bit_count * 4);
  const std::size_t cut_second = offset_of_significant(second.value(), k_mark_bit_count * 4);

  const std::string spliced = first->substr(0, cut_first) + second->substr(cut_second);

  const auto read = read_mark(spliced);

  ASSERT_FALSE(read.has_value());
  EXPECT_EQ(read.error(), CoreError::mark_not_confident);
}

} // namespace
