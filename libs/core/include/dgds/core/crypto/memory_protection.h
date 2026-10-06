#pragma once

#include <cstddef>

namespace dgds::core {

enum class MemoryLock {
  locked,
  failed,
};

enum class DumpPrevention {
  applied,
  unsupported,
  failed,
};

struct MemoryProtection {
  MemoryLock lock;
  DumpPrevention dump;

  bool operator==(const MemoryProtection &) const = default;
};

[[nodiscard]] MemoryProtection protect_memory(void *data, std::size_t size);
[[nodiscard]] MemoryLock release_memory(void *data, std::size_t size);

} // namespace dgds::core
