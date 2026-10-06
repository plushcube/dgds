#include <dgds/core/crypto/memory_protection.h>

#include <sys/mman.h>

#include <gtest/gtest.h>

#include <new>
#include <unistd.h>

#include <cstddef>
#include <limits>

namespace {

using dgds::core::DumpPrevention;
using dgds::core::MemoryLock;
using dgds::core::protect_memory;
using dgds::core::release_memory;

std::size_t page_size() { return static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)); }

TEST(MemoryProtection, AppliesProtectionToMappedRegion) {
  const std::size_t page = page_size();
  auto *data = static_cast<unsigned char *>(::operator new(page, std::align_val_t{page}));

  const auto protection = protect_memory(data, page);

  EXPECT_EQ(protection.lock, MemoryLock::locked);

#if defined(MADV_DONTDUMP)
  EXPECT_EQ(protection.dump, DumpPrevention::applied);
#else
  EXPECT_EQ(protection.dump, DumpPrevention::unsupported);
#endif

  EXPECT_EQ(release_memory(data, page), MemoryLock::locked);
  ::operator delete(data, std::align_val_t{page});
}

TEST(MemoryProtection, ClaimsNothingForEmptyRange) {
  const auto protection = protect_memory(nullptr, 0);

  EXPECT_EQ(protection.lock, MemoryLock::locked);
  EXPECT_EQ(protection.dump, DumpPrevention::unsupported);
  EXPECT_EQ(release_memory(nullptr, 0), MemoryLock::locked);
}

#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
#define DGDS_SANITIZED 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer)
#define DGDS_SANITIZED 1
#endif
#endif

TEST(MemoryProtection, ReportsRefusedCalls) {
#if defined(DGDS_SANITIZED)
  GTEST_SKIP() << "санитайзер перехватывает вызовы блокировки и сообщает успех там, где ядро отказало бы";
#elif defined(__linux__)
  const std::size_t page = page_size();
  const auto unmapped = reinterpret_cast<void *>(1);

  const auto protection = protect_memory(unmapped, page);

  EXPECT_EQ(protection.lock, MemoryLock::failed);
  EXPECT_EQ(protection.dump, DumpPrevention::failed);
  EXPECT_EQ(release_memory(unmapped, page), MemoryLock::failed);
#else
  constexpr std::size_t k_impossible = std::numeric_limits<std::size_t>::max();

  const std::size_t page = page_size();
  auto *data = static_cast<unsigned char *>(::operator new(page, std::align_val_t{page}));

  const auto protection = protect_memory(data, k_impossible);

  EXPECT_EQ(protection.lock, MemoryLock::failed);
  EXPECT_EQ(protection.dump, DumpPrevention::unsupported);
  EXPECT_EQ(release_memory(data, k_impossible), MemoryLock::failed);

  ::operator delete(data, std::align_val_t{page});
#endif
}

} // namespace
