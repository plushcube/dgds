#include "composition/ports.h"

#include <dgds/server/api/surface.h>
#include <dgds/server/http/binding.h>
#include <dgds/server/middleware/rate_limiter.h>
#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/version.h>

#include <httplib.h>

#include <chrono>
#include <iostream>
#include <ostream>
#include <string_view>

#include "composition/tls.h"

namespace {

constexpr std::string_view k_backend = DGDS_BACKEND_NAME;
constexpr dgds::core::Timestamp k_rate_window = 60;

dgds::core::Timestamp wall_clock() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void print_usage(std::ostream &out) {
  out << "dgds [--root <путь>] [--master-key <путь>] [--host <адрес>] [--port <номер>] [--rate-limit <вызовов>]\n"
      << "     --root         каталог данных сервера, по умолчанию ~/.dgds\n"
      << "     --master-key   ключ хранилища ключей, по умолчанию <каталог данных>/master.key\n"
      << "     --host         адрес прослушивания, по умолчанию 127.0.0.1\n"
      << "     --port         порт HTTPS, по умолчанию 8443; 0 — любой свободный\n"
      << "     --rate-limit   вызовов публикации и выдачи в минуту на пользователя, по умолчанию 100; 0 — без предела\n"
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

  const dgds::app::Configuration &configuration = command_line.configuration;
  const dgds::app::ServerPorts ports = dgds::app::make_server_ports(configuration);

  if (!ports.complete()) {
    std::cerr << "Собран неполный набор портов, сервер не запущен.\n";
    return 1;
  }

  const auto tls = dgds::app::load_or_create_certificate(configuration.tls_directory);

  if (!tls.has_value()) {
    std::cerr << "Не удалось подготовить сертификат сервера в " << configuration.tls_directory.string() << '\n';
    return 1;
  }

  const auto fingerprint = dgds::app::certificate_fingerprint(tls->certificate);

  if (!fingerprint.has_value()) {
    std::cerr << "Не удалось прочитать сертификат сервера.\n";
    return 1;
  }

  dgds::server::SessionStore sessions;
  dgds::server::UserService users{*ports.p_metadata, sessions};
  dgds::server::CatalogService catalog{*ports.p_metadata};
  dgds::server::PublicationService publications{*ports.p_identities, *ports.p_keys, *ports.p_blobs, *ports.p_metadata};
  dgds::server::PurchaseService purchases{*ports.p_keys, *ports.p_metadata};
  dgds::server::DeliveryService delivery{*ports.p_blobs, *ports.p_keys, *ports.p_metadata};
  dgds::server::RateLimiter limiter{
      dgds::server::RateLimit{.calls = configuration.rate_limit, .window = k_rate_window}};
  dgds::server::api::Surface surface{users, sessions, catalog, publications, purchases, delivery, limiter, wall_clock};

  httplib::SSLServer server{tls->certificate.c_str(), tls->key.c_str()};

  if (!server.is_valid()) {
    std::cerr << "Сертификат сервера не принят: " << tls->certificate.string() << '\n';
    return 1;
  }

  dgds::server::http::bind(server, surface);

  const int port = configuration.port == 0
                       ? server.bind_to_any_port(configuration.host)
                       : (server.bind_to_port(configuration.host, configuration.port) ? configuration.port : -1);

  if (port < 0) {
    std::cerr << "Не удалось занять " << configuration.host << ':' << configuration.port << '\n';
    return 1;
  }

  std::cout << "Набор адаптеров: " << k_backend << '\n'
            << "Каталог данных:  " << configuration.storage_root.string() << '\n'
            << "Сертификат:      " << tls->certificate.string() << '\n'
            << "Отпечаток:       " << fingerprint.value() << '\n'
            << "Слушаю https://" << configuration.host << ':' << port << '\n'
            << std::flush;

  server.listen_after_bind();

  return 0;
}
