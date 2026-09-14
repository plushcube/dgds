#include <dgds/core/watermark/mark_codec.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using dgds::core::CoreError;
using dgds::core::decode_mark;
using dgds::core::encode_mark;
using dgds::core::k_mark_version;
using dgds::core::Mark;
using dgds::core::MarkBits;

TEST(MarkCodec, RoundTripKeepsPurchaseId) {
  const Mark mark{.purchase_id = 42, .version = k_mark_version};

  const auto decoded = decode_mark(encode_mark(mark));

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value(), mark);
}

TEST(MarkCodec, RoundTripCoversRangeEnds) {
  for (const std::uint64_t id : {std::uint64_t{0}, std::numeric_limits<std::uint64_t>::max()}) {
    const Mark mark{.purchase_id = id, .version = k_mark_version};

    const auto decoded = decode_mark(encode_mark(mark));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value().purchase_id, id);
  }
}

TEST(MarkCodec, RejectsUnsupportedVersion) {
  const Mark mark{.purchase_id = 7, .version = k_mark_version + 1};

  const auto decoded = decode_mark(encode_mark(mark));

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), CoreError::mark_version_unsupported);
}

TEST(MarkCodec, DetectsCorruptedFrame) {
  const MarkBits encoded = encode_mark(Mark{.purchase_id = 7, .version = k_mark_version});

  for (const std::size_t position : {std::size_t{20}, std::size_t{0}}) {
    MarkBits bits = encoded;
    bits[position] = static_cast<std::uint8_t>(bits[position] ^ 1U);

    const auto decoded = decode_mark(bits);

    ASSERT_FALSE(decoded.has_value());
    EXPECT_EQ(decoded.error(), CoreError::mark_checksum_mismatch);
  }
}

TEST(MarkCodec, RejectsMalformedBits) {
  MarkBits bits = encode_mark(Mark{.purchase_id = 7, .version = k_mark_version});
  bits[80] = 2;

  const auto decoded = decode_mark(bits);

  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error(), CoreError::mark_malformed);
}

} // namespace
