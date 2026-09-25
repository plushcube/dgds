#include <dgds/client/client.h>
#include <dgds/client/http/http_transport.h>
#include <dgds/stubs/device_key/file_device_key.h>
#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/protocol.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/version.h>

#include <charconv>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace {

constexpr std::string_view k_author = "автор";
constexpr std::string_view k_buyer = "покупатель";
constexpr std::string_view k_title = "Демонстрационная публикация";
constexpr std::string_view k_file_name = "текст.txt";
constexpr const char *k_default_host = "127.0.0.1";
constexpr int k_default_port = 8443;

enum class Command {
  run,
  show_version,
  show_usage,
  invalid,
};

std::filesystem::path default_device_root() {
  return std::filesystem::temp_directory_path() / ("dgds-device-" + std::to_string(::getpid()));
}

struct Options {
  Command command = Command::run;
  std::string host = k_default_host;
  int port = k_default_port;
  std::filesystem::path certificate;
  std::filesystem::path device = default_device_root();
  std::optional<std::filesystem::path> text;
};

void print_usage(std::ostream &out) {
  out << "dgds-example [--certificate <файл>] [--host <адрес>] [--port <номер>] [--device <каталог>] [--text <файл>]\n"
      << "     --certificate  сертификат сервера: им проверяется и закрепляется соединение, обязателен\n"
      << "     --host         адрес сервера, по умолчанию " << k_default_host << "\n"
      << "     --port         порт сервера, по умолчанию " << k_default_port << "\n"
      << "     --device       каталог устройства: ключ и квитанции, по умолчанию " << default_device_root().string()
      << "\n"
      << "     --text         файл для публикации, по умолчанию встроенный образец\n"
      << "     --version      версия продукта\n"
      << "     --help         эта справка\n";
}

std::optional<int> number_of(std::string_view text) {
  int value = 0;
  const char *begin = text.data();
  const char *end = begin + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);

  if (parsed.ec != std::errc() || parsed.ptr != end) {
    return std::nullopt;
  }

  return value;
}

Options parse_arguments(int argc, char *argv[]) {
  Options options;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];

    if (argument == "--version") {
      options.command = Command::show_version;
      return options;
    }

    if (argument == "--help") {
      options.command = Command::show_usage;
      return options;
    }

    if (index + 1 >= argc) {
      options.command = Command::invalid;
      return options;
    }

    const std::string value = argv[++index];

    if (argument == "--host") {
      options.host = value;
      continue;
    }

    if (argument == "--port") {
      const std::optional<int> port = number_of(value);

      if (!port.has_value() || port.value() <= 0 || port.value() > 65535) {
        options.command = Command::invalid;
        return options;
      }

      options.port = port.value();
      continue;
    }

    if (argument == "--certificate") {
      options.certificate = value;
      continue;
    }

    if (argument == "--device") {
      options.device = value;
      continue;
    }

    if (argument == "--text") {
      options.text = value;
      continue;
    }

    options.command = Command::invalid;
    return options;
  }

  if (options.command == Command::run && options.certificate.empty()) {
    options.command = Command::invalid;
  }

  return options;
}

std::string sample_text() {
  std::string text;

  for (std::size_t line = 0; line < 20; ++line) {
    text += "строка " + std::to_string(line) + " демонстрационного текста для покупателя\n";
  }

  return text;
}

std::string published_text(const Options &options) {
  if (!options.text.has_value()) {
    return sample_text();
  }

  std::ifstream input(options.text.value(), std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
}

int report(std::string_view what, dgds::core::CoreError error) {
  std::cerr << "Не удалось выполнить: " << what << " (" << dgds::core::code_of(error) << ")\n";
  return 1;
}

} // namespace

int main(int argc, char *argv[]) {
  const Options options = parse_arguments(argc, argv);

  if (options.command == Command::show_version) {
    std::cout << dgds::k_version << '\n';
    return 0;
  }

  if (options.command == Command::show_usage) {
    print_usage(std::cout);
    return 0;
  }

  if (options.command == Command::invalid) {
    print_usage(std::cerr);
    return 1;
  }

  const auto trust = dgds::client::load_server_trust(options.certificate);

  if (!trust.has_value()) {
    return report("доверие сертификату сервера", trust.error());
  }

  dgds::client::HttpTransport transport{{.host = options.host, .port = options.port}, trust.value()};
  dgds::stubs::FileDeviceKey device_key{options.device / "device.key"};
  dgds::stubs::FileReceiptStore receipts{options.device / "receipts"};
  dgds::client::ApiClient client{transport, device_key, receipts};

  std::cout << "Сервер      " << options.host << ':' << options.port << "\n"
            << "Закрепление " << trust->pin.to_hex() << '\n';

  const auto author = client.register_user(k_author);

  if (!author.has_value()) {
    return report("регистрацию автора", author.error());
  }

  const auto author_credentials = client.log_in(k_author);

  if (!author_credentials.has_value()) {
    return report("вход автора", author_credentials.error());
  }

  const std::string text = published_text(options);
  const auto author_keys = dgds::core::generate_author_key();
  const auto identity = dgds::core::content_identity(text);

  if (!author_keys.has_value()) {
    return report("ключ автора", author_keys.error());
  }

  if (!identity.has_value()) {
    return report("идентичность контента", identity.error());
  }

  const auto signature = dgds::core::sign_author(identity.value(), author->name, author_keys->private_key);

  if (!signature.has_value()) {
    return report("подпись контента", signature.error());
  }

  const dgds::core::PublicationDraft draft{
      .title = std::string(k_title), .file_name = std::string(k_file_name), .content = text};
  const auto publication =
      client.publish(author_credentials.value(), draft, author_keys->public_key, signature.value());

  if (!publication.has_value()) {
    return report("публикацию", publication.error());
  }

  std::cout << "Публикация  " << publication.value() << " «" << k_title << "» автором «" << k_author << "»\n";

  const auto catalog = client.catalog();

  if (!catalog.has_value()) {
    return report("просмотр каталога", catalog.error());
  }

  std::cout << "Каталог     " << catalog->size() << " публикаций:\n";

  for (const auto &summary : catalog.value()) {
    std::cout << "            " << summary.publication_id << " «" << summary.title << "» " << summary.author_name
              << ", " << summary.size << " байт\n";
  }

  const auto buyer = client.register_user(k_buyer);

  if (!buyer.has_value()) {
    return report("регистрацию покупателя", buyer.error());
  }

  const auto buyer_credentials = client.log_in(k_buyer);

  if (!buyer_credentials.has_value()) {
    return report("вход покупателя", buyer_credentials.error());
  }

  const auto purchased = client.buy(buyer_credentials.value(), publication.value());

  if (!purchased.has_value()) {
    return report("покупку", purchased.error());
  }

  std::cout << "Покупка     " << purchased->header.purchase_id << " покупателем «" << k_buyer << "»\n";

  const auto content = client.fetch_content(buyer_credentials.value(), purchased->header.purchase_id);

  if (!content.has_value()) {
    return report("получение контента", content.error());
  }

  std::cout << "=== потребитель выводит полученный контент ===\n";
  std::cout << content->view();
  std::cout << "=== конец контента ===\n";

  return 0;
}
