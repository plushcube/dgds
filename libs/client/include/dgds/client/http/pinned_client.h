#pragma once

#include <dgds/client/http/server_trust.h>
#include <dgds/client/models/endpoint.h>
#include <dgds/core/models/errors.h>

#include <httplib.h>

#include <memory>

namespace dgds::client {

using HttpClient = std::unique_ptr<httplib::SSLClient>;

[[nodiscard]] core::Result<HttpClient> make_pinned_client(const Endpoint &endpoint, const ServerTrust &trust);

} // namespace dgds::client
