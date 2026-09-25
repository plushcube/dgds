#pragma once

#include <dgds/server/api/surface.h>
#include <dgds/server/middleware/rate_limiter.h>

#include <httplib.h>

namespace dgds::server::http {

void bind(httplib::Server &server, api::Surface &surface, RateLimiter &limiter, api::Surface::Clock clock);

} // namespace dgds::server::http
