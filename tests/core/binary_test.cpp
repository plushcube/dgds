#include <dgds/core/codec/binary.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

using dgds::core::append_integer;
using dgds::core::ContentBuffer;
using dgds::core::k_integer_size;
using dgds::core::Reader;

TEST(BinaryCodecTest, ClampsWidthBeyondInteger) {
  ContentBuffer data;
  append_integer(data, 0x0102030405060708ULL, k_integer_size + 8);

  ASSERT_EQ(data.size(), k_integer_size);

  Reader reader(data);

  std::uint64_t value = 0;
  ASSERT_TRUE(reader.read_integer(value, k_integer_size));
  EXPECT_EQ(value, 0x0102030405060708ULL);
}

TEST(BinaryCodecTest, RejectsWidthBeyondInteger) {
  Reader reader("abcdefghij");

  std::uint64_t value = 0;

  EXPECT_FALSE(reader.read_integer(value, k_integer_size + 1));
  EXPECT_EQ(reader.remaining(), 10U);
}

TEST(BinaryCodecTest, KeepsReaderIntactWhenDataIsMissing) {
  Reader reader("a");

  std::uint64_t value = 0;

  EXPECT_FALSE(reader.read_integer(value, 4));
  EXPECT_EQ(reader.remaining(), 1U);

  EXPECT_TRUE(reader.read_integer(value, 1));
  EXPECT_EQ(value, 0x61U);
  EXPECT_TRUE(reader.empty());
}

} // namespace
