#pragma once

#include <filesystem>

namespace dgds::app {

struct Configuration {
  std::filesystem::path storage_root;
  std::filesystem::path master_key;
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
