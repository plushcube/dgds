#include <http_client_fixture.h>

#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/models/protocol.h>

#include <httplib.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <thread>

namespace {

using dgds::client::Endpoint;
using dgds::client::HttpTransport;
using dgds::client::load_server_trust;
using dgds::core::canonical_form;
using dgds::core::CoreError;
using dgds::core::Credentials;
using dgds::core::generate_author_key;
using dgds::core::PublicationDraft;
using dgds::core::sign_author;
using dgds::test::HttpClientFixture;
using dgds::test::k_publication_title;
using dgds::test::sample_content;

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

TEST_F(HttpClientFixture, DeliversContentOfLimitSize) {
  const std::string text(dgds::core::k_max_content_bytes, 'x');

  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto receipt = m_client->buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value()) << dgds::core::code_of(receipt.error());

  const auto content = m_client->fetch_content(buyer, receipt->header.purchase_id);

  ASSERT_TRUE(content.has_value()) << dgds::core::code_of(content.error());
  EXPECT_GE(content->view().size(), text.size());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(HttpClientFixture, RunsPurchaseScenarioOverNetwork) {
  const std::string text = sample_content();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto catalog = m_client->catalog();
  ASSERT_TRUE(catalog.has_value());
  EXPECT_EQ(catalog->request.offset, 0U);
  EXPECT_EQ(catalog->request.limit, dgds::core::k_default_page_size);
  EXPECT_EQ(catalog->total, 1U);
  ASSERT_EQ(catalog->records.size(), 1U);
  EXPECT_EQ(catalog->records.front().publication_id, publication_id);
  EXPECT_EQ(catalog->records.front().title, std::string(k_publication_title));
  EXPECT_EQ(catalog->records.front().author_name, "автор");

  const auto receipt = m_client->buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());
  EXPECT_EQ(receipt->header.user_id, buyer.user_id);

  const auto purchases = m_client->purchases(buyer);
  ASSERT_TRUE(purchases.has_value());
  EXPECT_EQ(purchases->request.offset, 0U);
  EXPECT_EQ(purchases->request.limit, dgds::core::k_default_page_size);
  EXPECT_EQ(purchases->total, 1U);
  ASSERT_EQ(purchases->records.size(), 1U);
  EXPECT_EQ(purchases->records.front().publication.publication_id, publication_id);

  const auto content = m_client->fetch_content(buyer, receipt->header.purchase_id);
  ASSERT_TRUE(content.has_value()) << dgds::core::code_of(content.error());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(HttpClientFixture, ReturnsCatalogWindowOverNetwork) {
  ASSERT_NE(publish("автор", sample_content() + "первая"), 0U);
  ASSERT_NE(publish("автор", sample_content() + "вторая"), 0U);
  ASSERT_NE(publish("автор", sample_content() + "третья"), 0U);

  const auto first = m_transport->catalog(0, 2);

  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->request.offset, 0U);
  EXPECT_EQ(first->request.limit, 2U);
  EXPECT_EQ(first->total, 3U);
  ASSERT_EQ(first->records.size(), 2U);

  const auto last = m_transport->catalog(2, 2);

  ASSERT_TRUE(last.has_value());
  EXPECT_EQ(last->request.offset, 2U);
  EXPECT_EQ(last->request.limit, 2U);
  EXPECT_EQ(last->total, 3U);
  ASSERT_EQ(last->records.size(), 1U);
  EXPECT_LT(first->records.back().publication_id, last->records.front().publication_id);
}

TEST_F(HttpClientFixture, RestoresReceiptOverNetwork) {
  const std::string text = sample_content();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto receipt = m_client->buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());

  std::filesystem::remove_all(device() / "receipts");

  const auto content = m_client->fetch_content(buyer, receipt->header.purchase_id);
  ASSERT_TRUE(content.has_value()) << "квитанция не восстановлена с сервера";
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(HttpClientFixture, ReportsDuplicateName) {
  const auto first = m_client->register_user("автор");
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->name, "автор");

  const auto second = m_client->register_user("автор");
  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error(), CoreError::user_name_taken);
}

TEST_F(HttpClientFixture, ReportsDuplicateContent) {
  const std::string text = sample_content();
  ASSERT_NE(publish("автор", text), 0U);

  const auto credentials = m_client->log_in("автор");
  ASSERT_TRUE(credentials.has_value());

  const auto keys = generate_author_key();
  const auto identity = dgds::core::content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const PublicationDraft draft{.title = std::string(k_publication_title), .file_name = "файл.txt", .content = text};
  const auto duplicate = m_client->publish(credentials.value(), draft, keys->public_key, signature.value());

  ASSERT_FALSE(duplicate.has_value());
  EXPECT_EQ(duplicate.error(), CoreError::content_duplicate);
}

TEST_F(HttpClientFixture, ReportsMissingPublication) {
  const auto buyer = sign_in("покупатель");

  const auto receipt = m_client->buy(buyer, 999999);
  ASSERT_FALSE(receipt.has_value());
  EXPECT_EQ(receipt.error(), CoreError::publication_not_found);
}

TEST_F(HttpClientFixture, ReportsForeignToken) {
  const auto account = m_client->register_user("покупатель");
  ASSERT_TRUE(account.has_value());

  const Credentials credentials{.user_id = account->user_id, .token = "чужой"};
  const auto purchases = m_client->purchases(credentials);

  ASSERT_FALSE(purchases.has_value());
  EXPECT_EQ(purchases.error(), CoreError::authorization_failed);
}

TEST_F(HttpClientFixture, ReportsProtocolCodeItCannotMap) {
  const Credentials credentials{.user_id = {}, .token = "чужой"};
  const auto purchases = m_client->purchases(credentials);

  ASSERT_FALSE(purchases.has_value());
  EXPECT_EQ(purchases.error(), CoreError::protocol_failure);
}

TEST_F(HttpClientFixture, RefusesUnsupportedProtocolVersion) {
  VersionPeer peer{m_server.certificate(), m_server.root() / "tls" / "server.key"};
  ASSERT_TRUE(peer.start());

  const auto trust = load_server_trust(m_server.certificate());
  ASSERT_TRUE(trust.has_value());

  HttpTransport transport{Endpoint{.host = "127.0.0.1", .port = peer.port()}, trust.value()};
  const auto catalog = transport.catalog(0, dgds::core::k_default_page_size);

  ASSERT_FALSE(catalog.has_value());
  EXPECT_EQ(catalog.error(), CoreError::protocol_version_unsupported);
}

} // namespace
