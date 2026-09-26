#include <http_client_fixture.h>

#include <dgds/client/http/http_transport.h>

#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/models/protocol.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace {

using dgds::client::Endpoint;
using dgds::client::HttpTransport;
using dgds::client::load_server_trust;
using dgds::core::canonical_form;
using dgds::core::ContentIdentity;
using dgds::core::CoreError;
using dgds::core::Credentials;
using dgds::core::PurchaseId;
using dgds::test::HttpClientFixture;
using dgds::test::k_publication_line;
using dgds::test::sample_content;

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
}

void copy_tree(const std::filesystem::path &from, const std::filesystem::path &to) {
  std::filesystem::create_directories(to.parent_path());
  std::filesystem::copy(from, to, std::filesystem::copy_options::recursive);
}

class KeySeparationTest : public HttpClientFixture {
protected:
  void SetUp() override {
    HttpClientFixture::SetUp();

    const std::string text = sample_content();
    const auto publication_id = publish("автор", text);
    const auto buyer = sign_in("покупатель");

    const auto receipt = m_client->buy(buyer, publication_id);
    ASSERT_TRUE(receipt.has_value());
    m_purchase_id = receipt->header.purchase_id;

    const auto identity = dgds::core::content_identity(text);
    ASSERT_TRUE(identity.has_value());
    m_identity = identity.value();

    ASSERT_TRUE(m_client->fetch_content(buyer, m_purchase_id).has_value()) << "исходный стенд не выдал контент";
  }

  // Переносит хранилище контента и реестры в отдельный стенд; хранилище ключей — только по требованию
  void steal(bool with_keys) {
    const std::filesystem::path source = m_server.root();
    m_stolen_root = m_root / (with_keys ? "stolen-with-keys" : "stolen-without-keys");

    copy_tree(source / "blobs", m_stolen_root / "blobs");
    copy_tree(source / "metadata", m_stolen_root / "metadata");

    if (with_keys) {
      copy_tree(source / "keys", m_stolen_root / "keys");
      std::filesystem::copy_file(source / "master.key", m_stolen_root / "master.key");
    }

    ASSERT_TRUE(m_stolen.start(m_stolen_root));

    const auto trust = load_server_trust(m_stolen.certificate());
    ASSERT_TRUE(trust.has_value());

    m_stolen_transport =
        std::make_unique<HttpTransport>(Endpoint{.host = "127.0.0.1", .port = m_stolen.port()}, trust.value());
  }

  [[nodiscard]] Credentials stolen_credentials() {
    const auto credentials = m_stolen_transport->log_in("покупатель");
    EXPECT_TRUE(credentials.has_value());

    return credentials.value_or(Credentials{});
  }

  [[nodiscard]] bool content_store_holds_content() const {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(m_stolen_root / "blobs")) {
      if (!entry.is_regular_file()) {
        continue;
      }

      if (canonical_form(read_file(entry.path())).find(k_publication_line) != std::string::npos) {
        return true;
      }
    }

    return false;
  }

  std::filesystem::path m_stolen_root;
  dgds::test::ServerProcess m_stolen;
  std::unique_ptr<HttpTransport> m_stolen_transport;
  PurchaseId m_purchase_id = 0;
  ContentIdentity m_identity{};
};

TEST_F(KeySeparationTest, ContentStoreWithoutKeyStoreCannotBeDecrypted) {
  steal(false);

  const auto credentials = stolen_credentials();

  const auto catalog = m_stolen_transport->catalog(0, dgds::core::k_default_page_size);
  ASSERT_TRUE(catalog.has_value());
  EXPECT_EQ(catalog->size(), 1U);

  const auto purchases = m_stolen_transport->purchases(credentials, 0, dgds::core::k_default_page_size);
  ASSERT_TRUE(purchases.has_value());
  EXPECT_EQ(purchases->size(), 1U);

  const auto package = m_stolen_transport->fetch_package(credentials, m_purchase_id, device_public_key());

  ASSERT_FALSE(package.has_value()) << "хранилища контента хватило для выдачи";
  EXPECT_EQ(package.error(), CoreError::key_not_found);

  EXPECT_FALSE(content_store_holds_content()) << "в хранилище контента есть открытый текст";
}

TEST_F(KeySeparationTest, ContentStoreWithKeyStoreDeliversContent) {
  steal(true);

  const auto credentials = stolen_credentials();
  const auto package = m_stolen_transport->fetch_package(credentials, m_purchase_id, device_public_key());

  ASSERT_TRUE(package.has_value()) << dgds::core::code_of(package.error());
  EXPECT_EQ(package->identity, m_identity);
}

} // namespace
