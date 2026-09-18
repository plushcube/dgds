#include <dgds/core/models/crypto.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

namespace {

using dgds::core::SecretBytes;

constexpr std::size_t k_probe_size = 4;
using Probe = SecretBytes<k_probe_size>;

bool all_zero(const std::uint8_t *bytes, std::size_t size) {
  return std::all_of(bytes, bytes + size, [](std::uint8_t byte) { return byte == 0; });
}

void fill(Probe &probe, const char *value) { std::memcpy(probe.data(), value, k_probe_size); }

TEST(SecretBytes, WipesContentOnRequest) {
  Probe probe;
  fill(probe, "abcd");

  probe.wipe();

  EXPECT_TRUE(all_zero(probe.data(), probe.size()));
}

TEST(SecretBytes, WipesContentOnDestruction) {
  alignas(Probe) std::array<std::uint8_t, sizeof(Probe)> storage{};

  auto *probe = new (storage.data()) Probe();
  fill(*probe, "abcd");

  probe->~Probe();

  EXPECT_TRUE(all_zero(storage.data(), storage.size()));
}

TEST(SecretBytes, MovesContentAndWipesSource) {
  Probe source;
  fill(source, "abcd");

  Probe expected;
  fill(expected, "abcd");

  Probe target(std::move(source));

  EXPECT_TRUE(target.equals(expected));
  EXPECT_TRUE(all_zero(source.data(), source.size()));
}

TEST(SecretBytes, ComparesContent) {
  Probe first;
  Probe second;
  Probe third;

  fill(first, "abcd");
  fill(second, "abcd");
  fill(third, "abce");

  EXPECT_TRUE(first.equals(second));
  EXPECT_FALSE(first.equals(third));
}

} // namespace
