#include <dgds/core/crypto/memory_protection.h>

#include <sys/mman.h>

#include <cstddef>

namespace dgds::core {

MemoryProtection protect_memory(void *data, std::size_t size) {
  MemoryProtection protection{.lock = MemoryLock::locked, .dump = DumpPrevention::unsupported};

  if (size == 0) {
    return protection;
  }

  if (::mlock(data, size) != 0) {
    protection.lock = MemoryLock::failed;
  }

#if defined(MADV_DONTDUMP)
  if (::madvise(data, size, MADV_DONTDUMP) == 0) {
    protection.dump = DumpPrevention::applied;
  } else {
    protection.dump = DumpPrevention::failed;
  }
#endif

  return protection;
}

MemoryLock release_memory(void *data, std::size_t size) {
  if (size == 0) {
    return MemoryLock::locked;
  }

  if (::munlock(data, size) != 0) {
    return MemoryLock::failed;
  }

  return MemoryLock::locked;
}

} // namespace dgds::core
