#include "composition/ports.h"

#include <dgds/version.h>

#include <iostream>
#include <ostream>
#include <string_view>

namespace {

constexpr std::string_view k_backend = DGDS_BACKEND_NAME;

void print_usage(std::ostream &out) {
  out << "dgds [--root <путь>] [--master-key <путь>]\n"
      << "     --root         каталог данных сервера, по умолчанию ~/.dgds\n"
      << "     --master-key   ключ хранилища ключей, по умолчанию <каталог данных>/master.key\n"
      << "     --version      версия продукта и набор адаптеров\n"
      << "     --help         эта справка\n";
}

} // namespace

int main(int argc, char *argv[]) {
  const dgds::app::CommandLine command_line = dgds::app::parse_command_line(argc, argv);

  if (command_line.command == dgds::app::Command::show_version) {
    std::cout << dgds::k_version << " (" << k_backend << ")\n";
    return 0;
  }

  if (command_line.command == dgds::app::Command::show_usage) {
    print_usage(std::cout);
    return 0;
  }

  if (command_line.command == dgds::app::Command::invalid) {
    print_usage(std::cerr);
    return 1;
  }

  const dgds::app::ServerPorts ports = dgds::app::make_server_ports(command_line.configuration);

  if (!ports.complete()) {
    std::cerr << "Собран неполный набор портов, сервер не запущен.\n";
    return 1;
  }

  std::cout << "Набор адаптеров: " << k_backend << '\n'
            << "Каталог данных:  " << command_line.configuration.storage_root.string() << '\n'
            << "Ключ хранилища:  " << command_line.configuration.master_key.string() << '\n';

  return 0;
}
