#include <gtest/gtest.h>

namespace {

class DGDS_Fixture : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

} // namespace

TEST_F(DGDS_Fixture, AlwaysTrueTest) { EXPECT_EQ(true, true); }
