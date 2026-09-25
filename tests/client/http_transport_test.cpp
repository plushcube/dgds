#include "server_process.h"

#include <dgds/client/client.h>
#include <dgds/client/http/http_transport.h>

#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/protocol.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/stubs/device_key/file_device_key.h>
#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <httplib.h>

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>

namespace {

using dgds::client::ApiClient;
using dgds::client::Endpoint;
using dgds::client::HttpTransport;
using dgds::client::load_server_trust;
using dgds::core::canonical_form;
using dgds::core::CoreError;
using dgds::core::Credentials;
using dgds::core::generate_author_key;
using dgds::core::PublicationDraft;
using dgds::core::PublicationId;
using dgds::core::sign_author;
using dgds::test::ServerProcess;

constexpr std::string_view k_title = "название";
constexpr std::string_view k_line = "строка публикуемого текста для проверки\n";

std::string long_text() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += std::string(k_line);
  }

  return text;
}

class VersionPeer {
public:
  VersionPeer(const std::filesystem::path &certificate, const std::filesystem::path &key)
      : m_certificate(certificate.string()), m_key(key.string()), m_server(m_certificate.c_str(), m_key.c_str()) {}

  VersionPeer(const VersionPeer &) = delete;
  VersionPeer &operator=(const VersionPeer &) = delete;
  VersionPeer(VersionPeer &&) = delete;
  VersionPeer &operator=(VersionPeer &&) = delete;
  ~VersionPeer() { stop(); }

  [[nodiscard]] bool start() {
    m_server.Post(".*", [](const httplib::Request &, httplib::Response &response) {
      response.set_content(R"({"version":99,"data":{}})", "application/json");
    });

    m_port = m_server.bind_to_any_port("127.0.0.1");

    if (m_port <= 0) {
      return false;
    }

    m_thread = std::thread([this] { m_server.listen_after_bind(); });

    return true;
  }

  void stop() {
    m_server.stop();

    if (m_thread.joinable()) {
      m_thread.join();
    }
  }

  [[nodiscard]] int port() const { return m_port; }

private:
  std::string m_certificate;
  std::string m_key;
  httplib::SSLServer m_server;
  std::thread m_thread;
  int m_port = 0;
};

class HttpTransportFixture : public ::testing::Test {
protected:
  HttpTransportFixture() : m_root(temporary_root()) {}

  void SetUp() override {
    std::filesystem::create_directories(m_root);
    ASSERT_TRUE(m_server.start(m_root / "server"));

    const auto trust = load_server_trust(m_server.certificate());
    ASSERT_TRUE(trust.has_value());

    m_transport =
        std::make_unique<HttpTransport>(Endpoint{.host = "127.0.0.1", .port = m_server.port()}, trust.value());
    m_client = std::make_unique<ApiClient>(*m_transport, m_device_key, m_receipts);
  }

  void TearDown() override {
    m_client.reset();
    m_transport.reset();
    m_server.stop();
    std::filesystem::remove_all(m_root);
  }

  [[nodiscard]] Credentials sign_in(std::string_view name) {
    const auto account = m_client->register_user(name);

    if (!account.has_value()) {
      EXPECT_EQ(account.error(), CoreError::user_name_taken);
    }

    const auto credentials = m_client->log_in(name);
    EXPECT_TRUE(credentials.has_value());

    return credentials.value_or(Credentials{});
  }

  [[nodiscard]] PublicationId publish(std::string_view name, std::string_view text) {
    const auto credentials = sign_in(name);
    const auto keys = generate_author_key();
    EXPECT_TRUE(keys.has_value());

    const auto identity = dgds::core::content_identity(text);
    EXPECT_TRUE(identity.has_value());

    const auto signature = sign_author(identity.value(), name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    const PublicationDraft draft{.title = std::string(k_title), .file_name = "файл.txt", .content = std::string(text)};
    const auto publication = m_client->publish(credentials, draft, keys->public_key, signature.value());
    EXPECT_TRUE(publication.has_value());

    return publication.value_or(0);
  }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-http-client-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  std::filesystem::path m_root;
  ServerProcess m_server;
  dgds::stubs::FileDeviceKey m_device_key{m_root / "device.key"};
  dgds::stubs::FileReceiptStore m_receipts{m_root / "receipts"};
  std::unique_ptr<HttpTransport> m_transport;
  std::unique_ptr<ApiClient> m_client;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(HttpTransportFixture, RunsPurchaseScenarioOverNetwork) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto catalog = m_client->catalog();
  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->size(), 1U);
  EXPECT_EQ(catalog->front().publication_id, publication_id);
  EXPECT_EQ(catalog->front().title, std::string(k_title));
  EXPECT_EQ(catalog->front().author_name, "автор");

  const auto receipt = m_client->buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());
  EXPECT_EQ(receipt->header.user_id, buyer.user_id);

  const auto purchases = m_client->purchases(buyer);
  ASSERT_TRUE(purchases.has_value());
  ASSERT_EQ(purchases->size(), 1U);
  EXPECT_EQ(purchases->front().publication.publication_id, publication_id);

  const auto content = m_client->fetch_content(buyer, receipt->header.purchase_id);
  ASSERT_TRUE(content.has_value()) << dgds::core::code_of(content.error());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(HttpTransportFixture, RestoresReceiptOverNetwork) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto receipt = m_client->buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());

  std::filesystem::remove_all(m_root / "receipts");

  const auto content = m_client->fetch_content(buyer, receipt->header.purchase_id);
  ASSERT_TRUE(content.has_value()) << "квитанция не восстановлена с сервера";
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(HttpTransportFixture, ReportsDuplicateName) {
  const auto first = m_client->register_user("автор");
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->name, "автор");

  const auto second = m_client->register_user("автор");
  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error(), CoreError::user_name_taken);
}

TEST_F(HttpTransportFixture, ReportsDuplicateContent) {
  const std::string text = long_text();
  ASSERT_NE(publish("автор", text), 0U);

  const auto credentials = m_client->log_in("автор");
  ASSERT_TRUE(credentials.has_value());

  const auto keys = generate_author_key();
  const auto identity = dgds::core::content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const PublicationDraft draft{.title = std::string(k_title), .file_name = "файл.txt", .content = text};
  const auto duplicate = m_client->publish(credentials.value(), draft, keys->public_key, signature.value());

  ASSERT_FALSE(duplicate.has_value());
  EXPECT_EQ(duplicate.error(), CoreError::content_duplicate);
}

TEST_F(HttpTransportFixture, ReportsMissingPublication) {
  const auto buyer = sign_in("покупатель");

  const auto receipt = m_client->buy(buyer, 999999);
  ASSERT_FALSE(receipt.has_value());
  EXPECT_EQ(receipt.error(), CoreError::publication_not_found);
}

TEST_F(HttpTransportFixture, ReportsForeignToken) {
  const auto account = m_client->register_user("покупатель");
  ASSERT_TRUE(account.has_value());

  const Credentials credentials{.user_id = account->user_id, .token = "чужой"};
  const auto purchases = m_client->purchases(credentials);

  ASSERT_FALSE(purchases.has_value());
  EXPECT_EQ(purchases.error(), CoreError::authorization_failed);
}

TEST_F(HttpTransportFixture, ReportsProtocolCodeItCannotMap) {
  const Credentials credentials{.user_id = {}, .token = "чужой"};
  const auto purchases = m_client->purchases(credentials);

  ASSERT_FALSE(purchases.has_value());
  EXPECT_EQ(purchases.error(), CoreError::protocol_failure);
}

TEST_F(HttpTransportFixture, RefusesUnsupportedProtocolVersion) {
  VersionPeer peer{m_server.certificate(), m_server.root() / "tls" / "server.key"};
  ASSERT_TRUE(peer.start());

  const auto trust = load_server_trust(m_server.certificate());
  ASSERT_TRUE(trust.has_value());

  HttpTransport transport{Endpoint{.host = "127.0.0.1", .port = peer.port()}, trust.value()};
  const auto summaries = transport.catalog();

  ASSERT_FALSE(summaries.has_value());
  EXPECT_EQ(summaries.error(), CoreError::protocol_version_unsupported);
}

} // namespace
