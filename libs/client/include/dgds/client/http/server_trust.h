#pragma once

#include <dgds/client/models/server_pin.h>
#include <dgds/core/models/errors.h>

#include <cstddef>
#include <filesystem>
#include <span>

namespace dgds::client {

struct ServerTrust {
  ServerPin pin;
  std::filesystem::path certificate;
};

[[nodiscard]] core::Result<ServerTrust> load_server_trust(const std::filesystem::path &certificate);

[[nodiscard]] bool pinned_matches(const ServerPin &pin, std::span<const unsigned char> certificate_der);

} // namespace dgds::client
