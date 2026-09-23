#include <dgds/stubs/identity_registry/file_identity_registry.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <latch>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

using dgds::core::ClaimOutcome;
using dgds::core::ContentIdentity;
using dgds::core::CoreError;
using dgds::stubs::FileIdentityRegistry;

ContentIdentity make_identity(std::uint8_t seed) {
  ContentIdentity identity{};

  for (std::size_t index = 0; index < identity.size(); ++index) {
    identity[index] = static_cast<std::uint8_t>(seed + index);
  }

  return identity;
}

class IdentityRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_root = std::filesystem::temp_directory_path() /
             ("dgds-identity-registry-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(m_root);
  }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(IdentityRegistryTest, ClaimsFreeIdentityOnce) {
  FileIdentityRegistry registry(m_root);

  const auto first = registry.claim(make_identity(1));
  const auto second = registry.claim(make_identity(1));

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_EQ(first.value(), ClaimOutcome::claimed);
  EXPECT_EQ(second.value(), ClaimOutcome::already_claimed);
}

TEST_F(IdentityRegistryTest, KeepsClaimAcrossInstances) {
  const ContentIdentity identity = make_identity(5);

  FileIdentityRegistry first(m_root);
  const auto initial = first.claim(identity);

  ASSERT_TRUE(initial.has_value());
  ASSERT_EQ(initial.value(), ClaimOutcome::claimed);

  FileIdentityRegistry second(m_root);
  const auto repeated = second.claim(identity);

  ASSERT_TRUE(repeated.has_value());
  EXPECT_EQ(repeated.value(), ClaimOutcome::already_claimed);
}

TEST_F(IdentityRegistryTest, KeepsIdentitiesIndependent) {
  FileIdentityRegistry registry(m_root);

  const auto first = registry.claim(make_identity(7));
  const auto second = registry.claim(make_identity(9));

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  EXPECT_EQ(first.value(), ClaimOutcome::claimed);
  EXPECT_EQ(second.value(), ClaimOutcome::claimed);
}

TEST_F(IdentityRegistryTest, OnlyOneConcurrentClaimSucceeds) {
  constexpr std::size_t k_thread_count = 8;

  FileIdentityRegistry registry(m_root);
  const ContentIdentity identity = make_identity(3);

  std::atomic<std::size_t> claimed{0};
  std::atomic<std::size_t> rejected{0};
  std::atomic<std::size_t> failed{0};
  std::latch start(k_thread_count);

  std::vector<std::thread> threads;
  threads.reserve(k_thread_count);

  for (std::size_t index = 0; index < k_thread_count; ++index) {
    threads.emplace_back([&registry, &identity, &start, &claimed, &rejected, &failed] {
      start.arrive_and_wait();

      const auto outcome = registry.claim(identity);

      if (!outcome.has_value()) {
        ++failed;
        return;
      }

      if (outcome.value() == ClaimOutcome::claimed) {
        ++claimed;
        return;
      }

      ++rejected;
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(failed.load(), 0U);
  EXPECT_EQ(claimed.load(), 1U);
  EXPECT_EQ(rejected.load(), k_thread_count - 1);
}

TEST_F(IdentityRegistryTest, ReportsStorageFailure) {
  std::ofstream blocker(m_root / "не-каталог", std::ios::binary);
  blocker << "файл";
  blocker.close();

  FileIdentityRegistry registry(m_root / "не-каталог" / "claims");

  const auto outcome = registry.claim(make_identity(11));

  ASSERT_FALSE(outcome.has_value());
  EXPECT_EQ(outcome.error(), CoreError::storage_failed);
}

TEST_F(IdentityRegistryTest, CreatesDirectoryForMarkers) {
  FileIdentityRegistry registry(m_root / "nested" / "claims");

  const auto outcome = registry.claim(make_identity(12));

  ASSERT_TRUE(outcome.has_value());
  EXPECT_EQ(outcome.value(), ClaimOutcome::claimed);
}

} // namespace
