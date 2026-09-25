#pragma once

#include <dgds/core/models/errors.h>

#include <filesystem>
#include <string>

namespace dgds::app {

struct TlsFiles {
  std::filesystem::path certificate;
  std::filesystem::path key;
};

[[nodiscard]] core::Result<TlsFiles> load_or_create_certificate(const std::filesystem::path &directory);
[[nodiscard]] core::Result<std::string> certificate_fingerprint(const std::filesystem::path &certificate);

} // namespace dgds::app
