#include <dgds/core/identity/user_id.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace {

using dgds::core::from_uuid;
using dgds::core::generate_user_id;
using dgds::core::to_uuid;

constexpr std::string_view k_hex = "0123456789abcdef";

TEST(UserIdText, RoundTripsThroughText) {
  const auto user_id = generate_user_id();
  ASSERT_TRUE(user_id.has_value());

  const std::string text = to_uuid(user_id.value());
  const auto parsed = from_uuid(text);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed.value(), user_id.value());
}

TEST(UserIdText, PrintsCanonicalForm) {
  const auto user_id = generate_user_id();
  ASSERT_TRUE(user_id.has_value());

  const std::string text = to_uuid(user_id.value());

  ASSERT_EQ(text.size(), 36U);
  EXPECT_EQ(text[8], '-');
  EXPECT_EQ(text[13], '-');
  EXPECT_EQ(text[18], '-');
  EXPECT_EQ(text[23], '-');
  EXPECT_EQ(text[14], '4');
  EXPECT_NE(std::string("0123456789abcdef").find(text[19]), std::string::npos);

  for (std::size_t index = 0; index < text.size(); ++index) {
    if (index == 8 || index == 13 || index == 18 || index == 23) {
      continue;
    }

    EXPECT_NE(k_hex.find(text[index]), std::string::npos) << index;
  }
}

TEST(UserIdText, RejectsCorruptedForm) {
  const auto user_id = generate_user_id();
  ASSERT_TRUE(user_id.has_value());

  const std::string valid = to_uuid(user_id.value());

  EXPECT_TRUE(from_uuid(valid).has_value());
  EXPECT_FALSE(from_uuid("").has_value());
  EXPECT_FALSE(from_uuid(valid.substr(0, valid.size() - 1)).has_value());
  EXPECT_FALSE(from_uuid(valid + "0").has_value());
  EXPECT_FALSE(from_uuid(valid.substr(0, 8) + "0" + valid.substr(9)).has_value());
  EXPECT_FALSE(from_uuid("g" + valid.substr(1)).has_value());
  EXPECT_FALSE(from_uuid("A" + valid.substr(1)).has_value());

  std::string wrong_version = valid;
  wrong_version[14] = '3';
  EXPECT_FALSE(from_uuid(wrong_version).has_value());

  std::string wrong_variant = valid;
  wrong_variant[19] = '0';
  EXPECT_FALSE(from_uuid(wrong_variant).has_value());
}

} // namespace
