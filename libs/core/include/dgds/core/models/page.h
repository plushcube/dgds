#pragma once

#include <cstddef>
#include <vector>

namespace dgds::core {

struct PageRequest {
  std::size_t offset;
  std::size_t limit;
};

template <typename Record> struct Page {
  PageRequest request;
  std::size_t total;
  std::vector<Record> records;
};

} // namespace dgds::core
