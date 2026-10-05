#include <dgds/core/watermark/mark_codec.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using dgds::core::decode_mark;
using dgds::core::encode_mark;
using dgds::core::is_legacy_mark;
using dgds::core::k_mark_version;
using dgds::core::Mark;

Mark make_mark(std::uint64_t purchase_id, std::uint8_t seed) {
  Mark mark{.purchase_id = purchase_id, .code = {}, .version = k_mark_version};

  for (std::size_t index = 0; index < mark.code.size(); ++index) {
    mark.code[index] = static_cast<std::uint8_t>(seed + index);
  }

  return mark;
}

TEST(MarkCodec, RoundTripKeepsFrame) {
  const Mark mark = make_mark(42, 10);

  EXPECT_EQ(decode_mark(encode_mark(mark)), mark);
}

TEST(MarkCodec, RoundTripCoversRangeEnds) {
  for (const std::uint64_t id : {std::uint64_t{0}, std::numeric_limits<std::uint64_t>::max()}) {
    const Mark mark = make_mark(id, 20);

    EXPECT_EQ(decode_mark(encode_mark(mark)).purchase_id, id);
  }
}

TEST(MarkCodec, KeepsVersionVerbatim) {
  Mark mark = make_mark(7, 30);
  mark.version = 1;

  EXPECT_EQ(decode_mark(encode_mark(mark)).version, 1);
}

TEST(MarkCodec, DoesNotTreatCurrentFrameAsLegacy) { EXPECT_FALSE(is_legacy_mark(encode_mark(make_mark(7, 40)))); }

} // namespace
