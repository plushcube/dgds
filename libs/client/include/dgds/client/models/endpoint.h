#pragma once

#include <string>

namespace dgds::client {

struct Endpoint {
  std::string host;
  int port = 0;
};

} // namespace dgds::client
