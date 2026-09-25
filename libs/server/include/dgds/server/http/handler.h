#pragma once

#include <dgds/server/api/surface.h>

#include <string>

namespace dgds::server::http {

struct HttpResponse {
  int status = 200;
  std::string body;
};

[[nodiscard]] HttpResponse handle(api::Surface &surface, const std::string &path, const std::string &body);

} // namespace dgds::server::http
