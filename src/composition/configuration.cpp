#include "configuration.h"

#include <cstdlib>
#include <string_view>

namespace dgds::app {
namespace {

constexpr std::string_view k_root_option = "--root";
constexpr std::string_view k_master_key_option = "--master-key";
constexpr const char *k_storage_directory = ".dgds";
constexpr const char *k_master_key_name = "master.key";

std::filesystem::path home_directory() {
  const char *home = std::getenv("HOME");

  if (home == nullptr || *home == '\0') {
    return std::filesystem::current_path();
  }

  return home;
}

} // namespace

Configuration default_configuration() {
  const std::filesystem::path storage = home_directory() / k_storage_directory;

  return Configuration{.storage_root = storage, .master_key = storage / k_master_key_name};
}

CommandLine parse_command_line(int argc, char *argv[]) {
  CommandLine command_line{.configuration = default_configuration()};

  bool master_key_given = false;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];

    if (argument == "--version") {
      command_line.command = Command::show_version;
      return command_line;
    }

    if (argument == "--help") {
      command_line.command = Command::show_usage;
      return command_line;
    }

    if ((argument == k_root_option || argument == k_master_key_option) && index + 1 < argc) {
      const std::filesystem::path value = argv[++index];

      if (argument == k_root_option) {
        command_line.configuration.storage_root = value;

        if (!master_key_given) {
          command_line.configuration.master_key = value / k_master_key_name;
        }

        continue;
      }

      command_line.configuration.master_key = value;
      master_key_given = true;

      continue;
    }

    command_line.command = Command::invalid;
    return command_line;
  }

  return command_line;
}

} // namespace dgds::app
