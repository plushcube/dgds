#include "server_process.h"

#include <dgds/client/http/pinned_client.h>

#include <httplib.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace {

using dgds::client::Endpoint;
using dgds::client::HttpClient;
using dgds::client::load_server_trust;
using dgds::client::make_pinned_client;
using dgds::client::ServerPin;
using dgds::client::ServerTrust;
using dgds::test::ServerProcess;

class PinnedConnectionTest : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(m_own.start(ServerProcess::temporary_root("dgds-pin-own")));
    ASSERT_TRUE(m_other.start(ServerProcess::temporary_root("dgds-pin-other")));
  }

  void TearDown() override {
    m_own.stop();
    m_other.stop();
    std::filesystem::remove_all(m_own.root());
    std::filesystem::remove_all(m_other.root());
  }

  [[nodiscard]] HttpClient connect_to_own(const ServerTrust &trust) const {
    auto client = make_pinned_client(Endpoint{.host = "127.0.0.1", .port = m_own.port()}, trust);

    if (!client.has_value()) {
      EXPECT_TRUE(client.has_value()) << "клиент с закреплением не построен";
      return nullptr;
    }

    (*client)->set_connection_timeout(5);

    return std::move(client.value());
  }

  [[nodiscard]] static httplib::Result fetch(HttpClient &client) {
    return client->Post("/catalog", R"({"version":1})", "application/json");
  }

  ServerProcess m_own;
  ServerProcess m_other;
};

TEST_F(PinnedConnectionTest, AcceptsServerWithPinnedKey) {
  const auto trust = load_server_trust(m_own.certificate());
  ASSERT_TRUE(trust.has_value());

  HttpClient client = connect_to_own(trust.value());
  ASSERT_NE(client, nullptr);

  const auto response = fetch(client);
  ASSERT_TRUE(response) << "соединение не установлено: " << httplib::to_string(response.error());
  EXPECT_EQ(response->status, 200);
}

TEST_F(PinnedConnectionTest, RefusesSubstitutedCertificate) {
  const auto own = load_server_trust(m_own.certificate());
  const auto other = load_server_trust(m_other.certificate());
  ASSERT_TRUE(own.has_value());
  ASSERT_TRUE(other.has_value());
  ASSERT_NE(own->pin, other->pin);

  HttpClient client = connect_to_own(other.value());
  ASSERT_NE(client, nullptr);

  const auto response = fetch(client);
  ASSERT_FALSE(response) << "подменённый сертификат принят";
  EXPECT_EQ(response.error(), httplib::Error::SSLServerVerification);
}

TEST_F(PinnedConnectionTest, RefusesKeyThatDoesNotMatchPin) {
  const auto own = load_server_trust(m_own.certificate());
  const auto other = load_server_trust(m_other.certificate());
  ASSERT_TRUE(own.has_value());
  ASSERT_TRUE(other.has_value());

  const ServerTrust substituted{.pin = other->pin, .certificate = own->certificate};
  HttpClient client = connect_to_own(substituted);
  ASSERT_NE(client, nullptr);

  const auto response = fetch(client);
  ASSERT_FALSE(response) << "чужой ключ принят при доверии сертификату сервера";
  EXPECT_EQ(response.error(), httplib::Error::SSLServerVerification);
}

TEST_F(PinnedConnectionTest, RejectsUnreadableTrust) {
  const std::filesystem::path absent = m_own.root() / "tls" / "absent.crt";

  EXPECT_FALSE(load_server_trust(absent).has_value());

  const std::filesystem::path garbage = m_own.root() / "garbage.crt";
  std::ofstream(garbage) << "не сертификат\n";

  EXPECT_FALSE(load_server_trust(garbage).has_value());

  const ServerTrust broken{.pin = ServerPin{}, .certificate = absent};

  EXPECT_FALSE(make_pinned_client(Endpoint{.host = "127.0.0.1", .port = m_own.port()}, broken).has_value());
}

} // namespace
