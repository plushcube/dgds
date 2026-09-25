#include "configuration.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>

namespace dgds::app {
namespace {

constexpr std::string_view k_root_option = "--root";
constexpr std::string_view k_master_key_option = "--master-key";
constexpr std::string_view k_host_option = "--host";
constexpr std::string_view k_port_option = "--port";
constexpr std::string_view k_rate_limit_option = "--rate-limit";
constexpr const char *k_storage_directory = ".dgds";
constexpr const char *k_master_key_name = "master.key";
constexpr const char *k_tls_directory_name = "tls";
constexpr const char *k_default_host = "127.0.0.1";
constexpr int k_default_port = 8443;
constexpr std::size_t k_default_rate_limit = 100;

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

  return Configuration{.storage_root = storage,
                       .master_key = storage / k_master_key_name,
                       .tls_directory = storage / k_tls_directory_name,
                       .host = k_default_host,
                       .port = k_default_port,
                       .rate_limit = k_default_rate_limit};
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
        command_line.configuration.tls_directory = value / k_tls_directory_name;

        if (!master_key_given) {
          command_line.configuration.master_key = value / k_master_key_name;
        }

        continue;
      }

      command_line.configuration.master_key = value;
      master_key_given = true;

      continue;
    }

    if ((argument == k_host_option || argument == k_port_option || argument == k_rate_limit_option) &&
        index + 1 < argc) {
      const std::string value = argv[++index];

      if (argument == k_host_option) {
        command_line.configuration.host = value;
        continue;
      }

      std::uint32_t number = 0;
      const std::from_chars_result parsed = std::from_chars(value.data(), value.data() + value.size(), number);

      const bool exact = parsed.ec == std::errc() && parsed.ptr == value.data() + value.size();
      const bool fits_port = number <= 65535;

      if (!exact || (argument == k_port_option && !fits_port)) {
        command_line.command = Command::invalid;
        return command_line;
      }

      if (argument == k_port_option) {
        command_line.configuration.port = static_cast<int>(number);
        continue;
      }

      command_line.configuration.rate_limit = number;
      continue;
    }

    command_line.command = Command::invalid;
    return command_line;
  }

  return command_line;
}

} // namespace dgds::app
