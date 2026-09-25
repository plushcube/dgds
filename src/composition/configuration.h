#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace dgds::app {

struct Configuration {
  std::filesystem::path storage_root;
  std::filesystem::path master_key;
  std::filesystem::path tls_directory;
  std::string host;
  int port;
  std::size_t rate_limit;
};

enum class Command {
  start,
  show_version,
  show_usage,
  invalid,
};

struct CommandLine {
  Command command = Command::start;
  Configuration configuration;
};

[[nodiscard]] Configuration default_configuration();
[[nodiscard]] CommandLine parse_command_line(int argc, char *argv[]);

} // namespace dgds::app
