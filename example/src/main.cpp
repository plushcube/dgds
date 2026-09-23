#include <dgds/client/client.h>
#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>
#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/device_key/file_device_key.h>
#include <dgds/stubs/direct_transport/direct_transport.h>
#include <dgds/stubs/identity_registry/file_identity_registry.h>
#include <dgds/stubs/key_store/file_key_store.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>
#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/version.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

constexpr std::string_view k_author = "автор";
constexpr std::string_view k_buyer = "покупатель";
constexpr std::string_view k_title = "Демонстрационная публикация";
constexpr std::string_view k_file_name = "текст.txt";

enum class Command {
  run,
  show_version,
  show_usage,
  invalid,
};

std::filesystem::path default_root() {
  return std::filesystem::temp_directory_path() / ("dgds-example-" + std::to_string(::getpid()));
}

struct Options {
  Command command = Command::run;
  std::filesystem::path root = default_root();
  std::optional<std::filesystem::path> text;
};

void print_usage(std::ostream &out) {
  out << "dgds-example [--root <путь>] [--text <файл>]\n"
      << "     --root   каталог стенда: сервер в <root>/server, устройство в <root>/device;\n"
      << "               стенд создаётся один раз, повторный запуск в занятом каталоге не предусмотрен;\n"
      << "               по умолчанию " << default_root().string() << "\n"
      << "     --text   файл для публикации, по умолчанию встроенный образец\n"
      << "     --version  версия продукта\n"
      << "     --help   эта справка\n";
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

    if (argument == "--root" && index + 1 < argc) {
      options.root = argv[++index];
      continue;
    }

    if (argument == "--text" && index + 1 < argc) {
      options.text = argv[++index];
      continue;
    }

    options.command = Command::invalid;
    return options;
  }

  return options;
}

dgds::core::Timestamp wall_clock() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
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

int report(std::string_view what) {
  std::cerr << "Не удалось выполнить: " << what << '\n';
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

  const std::filesystem::path server_root = options.root / "server";
  const std::filesystem::path device_root = options.root / "device";

  dgds::stubs::FileIdentityRegistry identities{server_root / "identities"};
  dgds::stubs::FileKeyStore keys{server_root / "master.key", server_root / "keys"};
  dgds::stubs::FileBlobStore blobs{server_root / "blobs"};
  dgds::stubs::FileMetadataRegistry metadata{server_root / "metadata"};
  dgds::server::SessionStore sessions;
  dgds::server::UserService users{metadata, sessions};
  dgds::server::CatalogService catalog{metadata};
  dgds::server::PublicationService publications{identities, keys, blobs, metadata};
  dgds::server::PurchaseService purchases{keys, metadata};
  dgds::server::DeliveryService delivery{blobs, keys, metadata};
  dgds::stubs::DirectTransport transport{users, sessions, catalog, publications, purchases, delivery, wall_clock};

  dgds::stubs::FileDeviceKey device_key{device_root / "device.key"};
  dgds::stubs::FileReceiptStore receipts{device_root / "receipts"};
  dgds::client::ApiClient client{transport, device_key, receipts};

  const auto author = client.register_user(k_author);

  if (!author.has_value()) {
    return report("регистрацию автора");
  }

  const auto author_credentials = client.log_in(k_author);

  if (!author_credentials.has_value()) {
    return report("вход автора");
  }

  const std::string text = published_text(options);
  const auto author_keys = dgds::core::generate_author_key();
  const auto identity = dgds::core::content_identity(text);

  if (!author_keys.has_value() || !identity.has_value()) {
    return report("подготовку ключа автора");
  }

  const auto signature = dgds::core::sign_author(identity.value(), author->name, author_keys->private_key);

  if (!signature.has_value()) {
    return report("подпись контента");
  }

  const dgds::core::PublicationDraft draft{
      .title = std::string(k_title), .file_name = std::string(k_file_name), .content = text};
  const auto publication =
      client.publish(author_credentials.value(), draft, author_keys->public_key, signature.value());

  if (!publication.has_value()) {
    return report("публикацию");
  }

  std::cout << "Публикация  " << publication.value() << " «" << k_title << "»\n";

  const auto buyer = client.register_user(k_buyer);

  if (!buyer.has_value()) {
    return report("регистрацию покупателя");
  }

  const auto buyer_credentials = client.log_in(k_buyer);

  if (!buyer_credentials.has_value()) {
    return report("вход покупателя");
  }

  const auto purchased = client.buy(buyer_credentials.value(), publication.value());

  if (!purchased.has_value()) {
    return report("покупку");
  }

  std::cout << "Покупка    " << purchased->header.purchase_id << " покупателем «" << k_buyer << "»\n";

  const auto content = client.fetch_content(buyer_credentials.value(), purchased->header.purchase_id);

  if (!content.has_value()) {
    return report("получение контента");
  }

  std::cout << "=== потребитель выводит полученный контент ===\n";
  std::cout << content->view();
  std::cout << "=== конец контента ===\n";

  return 0;
}
