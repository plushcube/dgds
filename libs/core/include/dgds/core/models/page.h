#pragma once

#include <cstddef>
#include <vector>

namespace dgds::core {

template <typename Record> struct Page {
  std::size_t total;
  std::vector<Record> records;
};

struct PageRequest {
  std::size_t offset;
  std::size_t limit;
};

} // namespace dgds::core
