#include <dgds/server/http/binding.h>

#include <dgds/server/http/handler.h>

#include <dgds/core/models/protocol.h>

#include <string>

namespace dgds::server::http {

void bind(httplib::Server &server, api::Surface &surface) {
  server.set_payload_max_length(core::k_max_request_bytes);

  const auto serve = [&surface](const httplib::Request &request, httplib::Response &response) {
    const HttpResponse handled = handle(surface, request.path, request.body);

    response.status = handled.status;
    response.set_content(handled.body, "application/json");
  };

  server.Post(".*", serve);
  server.Get(".*", serve);
}

} // namespace dgds::server::http
