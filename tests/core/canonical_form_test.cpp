#include <dgds/core/identity/canonical_form.h>

#include <gtest/gtest.h>

#include <string>

namespace {

using dgds::core::canonical_form;

TEST(CanonicalForm, RemovesZeroWidthSpace) { EXPECT_EQ(canonical_form("при\u200Bвет"), "привет"); }

TEST(CanonicalForm, RemovesVariationSelectorAfterAscii) { EXPECT_EQ(canonical_form("a\uFE0Fb"), "ab"); }

TEST(CanonicalForm, RemovesSupplementaryVariationSelectorAfterAscii) {
  EXPECT_EQ(canonical_form("a\U000E0100b"), "ab");
}

TEST(CanonicalForm, KeepsVariationSelectorOfEmoji) { EXPECT_EQ(canonical_form("\u2764\uFE0F"), "\u2764\uFE0F"); }

TEST(CanonicalForm, KeepsZeroWidthJoiner) {
  EXPECT_EQ(canonical_form("\U0001F468\u200D\U0001F469"), "\U0001F468\u200D\U0001F469");
}

TEST(CanonicalForm, KeepsContentWithoutChannel) {
  const std::string text = "DGDS, текст и эмодзи \U0001F381";

  EXPECT_EQ(canonical_form(text), text);
}

TEST(CanonicalForm, KeepsInvalidBytes) {
  const std::string binary("\xFF\xFE\x00binary", 9);

  EXPECT_EQ(canonical_form(binary), binary);
}

TEST(CanonicalForm, KeepsLineEndings) {
  const std::string windows = "a\r\nb";
  const std::string unix = "a\nb";

  EXPECT_EQ(canonical_form(windows), windows);
  EXPECT_EQ(canonical_form(unix), unix);
  EXPECT_NE(canonical_form(windows), canonical_form(unix));
}

TEST(CanonicalForm, IsIdempotent) {
  const std::string marked = "фа\u200Bйл с мет\uFE0Fками";

  const std::string once = canonical_form(marked);

  EXPECT_EQ(canonical_form(once), once);
}

TEST(CanonicalForm, RemovesEveryMarkOfTheChannel) { EXPECT_EQ(canonical_form("a\u200B\uFE0F\u200Bb\u200B"), "ab"); }

} // namespace
