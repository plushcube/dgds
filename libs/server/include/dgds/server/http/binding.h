#pragma once

#include <dgds/server/api/surface.h>

#include <httplib.h>

namespace dgds::server::http {

void bind(httplib::Server &server, api::Surface &surface);

} // namespace dgds::server::http
