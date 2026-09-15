#include <dgds/core/crypto/secure_buffer.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

namespace {

using dgds::core::SecureBuffer;

constexpr const char k_secret[] = "секрет";
constexpr std::size_t k_secret_size = sizeof(k_secret) - 1;

void fill(SecureBuffer &buffer) { std::memcpy(buffer.data(), k_secret, k_secret_size); }

TEST(SecureBuffer, KeepsContentUntilWipe) {
  SecureBuffer buffer(k_secret_size);
  fill(buffer);

  EXPECT_EQ(buffer.view(), k_secret);
  EXPECT_EQ(buffer.size(), k_secret_size);
}

TEST(SecureBuffer, WipesContentOnRequest) {
  SecureBuffer buffer(k_secret_size);
  fill(buffer);

  buffer.wipe();

  EXPECT_TRUE(std::all_of(buffer.data(), buffer.data() + buffer.size(), [](unsigned char byte) { return byte == 0; }));
}

TEST(SecureBuffer, HandsContentOverByMove) {
  SecureBuffer buffer(k_secret_size);
  fill(buffer);

  const auto consumer = [](SecureBuffer handed) { return std::string(handed.view()); };

  EXPECT_EQ(consumer(std::move(buffer)), k_secret);
  EXPECT_TRUE(buffer.empty());
}

} // namespace
