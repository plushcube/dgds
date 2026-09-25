#include <dgds/server/http/binding.h>

#include <dgds/server/http/handler.h>

#include <dgds/core/models/protocol.h>

#include <string>
#include <string_view>

namespace dgds::server::http {

void bind(httplib::Server &server, api::Surface &surface, RateLimiter &limiter, api::Surface::Clock clock) {
  server.set_payload_max_length(core::k_max_request_bytes);

  const auto serve = [&surface, &limiter, clock](const httplib::Request &request, httplib::Response &response) {
    if (!limiter.accepted(LimitedOperation::request, std::string_view(request.remote_addr), clock())) {
      const api::SurfaceResult rejected = api::failure(api::ProtocolError::rate_limit_exceeded);

      response.status = 429;
      response.set_content(rejected.body, "application/json");
      return;
    }

    const HttpResponse handled = handle(surface, request.path, request.body);

    response.status = handled.status;
    response.set_content(handled.body, "application/json");
  };

  server.Post(".*", serve);
  server.Get(".*", serve);
}

} // namespace dgds::server::http
