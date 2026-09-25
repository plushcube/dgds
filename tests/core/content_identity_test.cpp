#include <dgds/core/identity/content_identity.h>

#include <gtest/gtest.h>

namespace {

using dgds::core::content_identity;
using dgds::core::from_hex;
using dgds::core::to_hex;

TEST(ContentIdentity, MarksDoNotChangeIdentity) {
  const auto marked = content_identity("тек\u200Bст");
  const auto plain = content_identity("текст");

  ASSERT_TRUE(marked.has_value());
  ASSERT_TRUE(plain.has_value());

  EXPECT_EQ(marked.value(), plain.value());
}

TEST(ContentIdentity, DifferentContentGivesDifferentIdentity) {
  const auto first = content_identity("первый файл");
  const auto second = content_identity("второй файл");

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_NE(first.value(), second.value());
}

TEST(ContentIdentity, ContentOfMarksMatchesEmptyContent) {
  const auto marks = content_identity("\u200B\u200B");
  const auto empty = content_identity("");

  ASSERT_TRUE(marks.has_value());
  ASSERT_TRUE(empty.has_value());

  EXPECT_EQ(marks.value(), empty.value());
}

TEST(ContentIdentity, MatchesKnownDigestOfAbc) {
  const auto identity = content_identity("abc");

  ASSERT_TRUE(identity.has_value());

  EXPECT_EQ(to_hex(identity.value()), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(ContentIdentity, RoundTripsHex) {
  const std::string raw = "текст для кругового перехода";
  const std::string encoded = to_hex(reinterpret_cast<const std::uint8_t *>(raw.data()), raw.size());

  const auto decoded = from_hex(encoded);

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value(), raw);

  EXPECT_FALSE(from_hex("abc").has_value());
  EXPECT_FALSE(from_hex("zz").has_value());
}

} // namespace
