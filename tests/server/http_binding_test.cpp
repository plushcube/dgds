#include <dgds/server/api/surface.h>
#include <dgds/server/http/binding.h>
#include <dgds/server/middleware/rate_limiter.h>
#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/user_id.h>
#include <dgds/core/models/protocol.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/identity_registry/file_identity_registry.h>
#include <dgds/stubs/key_store/file_key_store.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

using Json = nlohmann::json;

using dgds::core::content_identity;
using dgds::core::generate_author_key;
using dgds::core::k_protocol_version;
using dgds::core::sign_author;
using dgds::core::to_hex;
using dgds::server::CatalogService;
using dgds::server::DeliveryService;
using dgds::server::PublicationService;
using dgds::server::PurchaseService;
using dgds::server::RateLimiter;
using dgds::server::SessionStore;
using dgds::server::UserService;
using dgds::server::api::Surface;
using dgds::server::http::bind;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileIdentityRegistry;
using dgds::stubs::FileKeyStore;
using dgds::stubs::FileMetadataRegistry;

constexpr std::string_view k_text = "строка публикуемого текста для проверки поверхности\n";
constexpr std::string_view k_title = "название";

std::string long_text() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += std::string(k_text);
  }

  return text;
}

class HttpBindingTest : public ::testing::Test {
protected:
  HttpBindingTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void SetUp() override {
    bind(m_server, m_surface);

    m_port = m_server.bind_to_any_port("127.0.0.1");
    ASSERT_GT(m_port, 0);

    m_worker = std::thread{[this] { m_server.listen_after_bind(); }};
    m_server.wait_until_ready();
  }

  void TearDown() override {
    m_server.stop();

    if (m_worker.joinable()) {
      m_worker.join();
    }

    std::filesystem::remove_all(m_root);
  }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-http-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] httplib::Result post(const std::string &path, const Json &body) {
    httplib::Client client{"127.0.0.1", m_port};
    client.set_connection_timeout(5);

    return client.Post(path, with_version(body).dump(), "application/json");
  }

  [[nodiscard]] static Json with_version(Json body) {
    Json request = std::move(body);

    if (!request.contains("version")) {
      request["version"] = k_protocol_version;
    }

    return request;
  }

  std::filesystem::path m_root;
  FileIdentityRegistry m_identities{m_root / "identities"};
  FileKeyStore m_keys{m_root / "master.key", m_root / "keys"};
  FileBlobStore m_blobs{m_root / "blobs"};
  FileMetadataRegistry m_metadata{m_root / "metadata"};
  SessionStore m_sessions;
  UserService m_users{m_metadata, m_sessions};
  CatalogService m_catalog{m_metadata};
  PublicationService m_publications{m_identities, m_keys, m_blobs, m_metadata};
  PurchaseService m_purchases{m_keys, m_metadata};
  DeliveryService m_delivery{m_blobs, m_keys, m_metadata};
  RateLimiter m_limiter;
  dgds::core::Timestamp m_now = 1700000000;
  Surface m_surface{m_users,     m_sessions, m_catalog, m_publications,
                    m_purchases, m_delivery, m_limiter, [this]() { return m_now; }};

  httplib::Server m_server;
  std::thread m_worker;
  int m_port = 0;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(HttpBindingTest, ServesScenarioOverHttp) {
  const auto registered = post("/register", Json{{"name", "автор"}});
  ASSERT_TRUE(registered);
  ASSERT_EQ(registered->status, 200);
  EXPECT_EQ(registered->get_header_value("Content-Type"), "application/json");

  const Json account = Json::parse(registered->body).at("data");
  EXPECT_EQ(account.at("name").get<std::string>(), "автор");

  const auto logged = post("/login", Json{{"name", "автор"}});
  ASSERT_TRUE(logged);
  ASSERT_EQ(logged->status, 200);

  const Json credentials = Json::parse(logged->body).at("data");
  EXPECT_TRUE(dgds::core::from_uuid(credentials.at("user_id").get<std::string>()).has_value());

  const std::string text = long_text();
  const auto keys = generate_author_key();
  const auto identity = content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const Json publication{{"credentials", credentials},
                         {"draft", Json{{"title", std::string(k_title)}, {"file_name", "файл.txt"}, {"content", text}}},
                         {"author_key", to_hex(keys->public_key.data(), keys->public_key.size())},
                         {"signature", to_hex(signature->data(), signature->size())}};

  const auto published = post("/publish", publication);
  ASSERT_TRUE(published);
  ASSERT_EQ(published->status, 200);

  const std::string publication_id = Json::parse(published->body).at("data").get<std::string>();

  const auto catalog = post("/catalog", Json::object());

  ASSERT_TRUE(catalog);
  ASSERT_EQ(catalog->status, 200);
  ASSERT_EQ(Json::parse(catalog->body).at("data").size(), 1U);
  EXPECT_EQ(Json::parse(catalog->body).at("data").at(0).at("publication_id").get<std::string>(), publication_id);

  const auto duplicate = post("/publish", publication);
  ASSERT_TRUE(duplicate);
  EXPECT_EQ(duplicate->status, 409);
  EXPECT_EQ(Json::parse(duplicate->body).at("error").at("code").get<std::string>(), "content_duplicate");

  const auto unknown = post("/не-такая-операция", Json::object());
  ASSERT_TRUE(unknown);
  EXPECT_EQ(unknown->status, 404);
  EXPECT_EQ(Json::parse(unknown->body).at("error").at("code").get<std::string>(), "operation_unknown");

  Json wrong_version = Json{{"name", "второй"}};
  wrong_version["version"] = 99;
  const auto rejected = post("/register", wrong_version);
  ASSERT_TRUE(rejected);
  EXPECT_EQ(rejected->status, 400);

  Json forged = credentials;
  forged["token"] = "поддельный";
  const auto unauthorized = post("/purchases", Json{{"credentials", forged}});
  ASSERT_TRUE(unauthorized);
  EXPECT_EQ(unauthorized->status, 401);
}

} // namespace
