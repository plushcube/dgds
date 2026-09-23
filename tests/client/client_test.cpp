#include <dgds/client/client.h>
#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/author.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/device_key/file_device_key.h>
#include <dgds/stubs/direct_transport/direct_transport.h>
#include <dgds/stubs/identity_registry/file_identity_registry.h>
#include <dgds/stubs/key_store/file_key_store.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>
#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using dgds::client::ApiClient;
using dgds::core::canonical_form;
using dgds::core::content_identity;
using dgds::core::CoreError;
using dgds::core::Credentials;
using dgds::core::generate_author_key;
using dgds::core::PublicationDraft;
using dgds::core::sign_author;
using dgds::server::CatalogService;
using dgds::server::DeliveryService;
using dgds::server::PublicationService;
using dgds::server::PurchaseService;
using dgds::server::SessionStore;
using dgds::server::UserService;
using dgds::stubs::DirectTransport;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileDeviceKey;
using dgds::stubs::FileIdentityRegistry;
using dgds::stubs::FileKeyStore;
using dgds::stubs::FileMetadataRegistry;
using dgds::stubs::FileReceiptStore;

constexpr std::string_view k_title = "название";
constexpr std::string_view k_text = "строка публикуемого текста для проверки\n";

std::string long_text() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += std::string(k_text);
  }

  return text;
}

class ClientTest : public ::testing::Test {
protected:
  ClientTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-client-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] Credentials sign_in(std::string_view name) {
    const auto account = m_client.register_user(name);
    EXPECT_TRUE(account.has_value());

    const auto credentials = m_client.log_in(name);
    EXPECT_TRUE(credentials.has_value());

    return credentials.value_or(Credentials{});
  }

  [[nodiscard]] dgds::core::PublicationId publish(std::string_view name, std::string_view text) {
    const auto credentials = sign_in(name);
    const auto keys = generate_author_key();
    EXPECT_TRUE(keys.has_value());

    const auto identity = content_identity(text);
    EXPECT_TRUE(identity.has_value());

    const auto signature = sign_author(identity.value(), name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    const PublicationDraft draft{.title = std::string(k_title), .file_name = "файл.txt", .content = std::string(text)};
    const auto publication = m_client.publish(credentials, draft, keys->public_key, signature.value());
    EXPECT_TRUE(publication.has_value());

    return publication.value_or(0);
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

  dgds::core::Timestamp m_now = 1700000000;
  DirectTransport m_transport{
      m_users, m_sessions, m_catalog, m_publications, m_purchases, m_delivery, [this]() { return m_now; }};

  FileDeviceKey m_device_key{m_root / "device.key"};
  FileReceiptStore m_receipts{m_root / "receipts"};
  ApiClient m_client{m_transport, m_device_key, m_receipts};

  FileDeviceKey m_second_key{m_root / "second.key"};
  FileReceiptStore m_second_receipts{m_root / "second-receipts"};
  ApiClient m_second_client{m_transport, m_second_key, m_second_receipts};

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(ClientTest, RunsFullPurchaseScenario) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto catalog = m_client.catalog();
  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->size(), 1U);
  EXPECT_EQ(catalog->front().publication_id, publication_id);

  const auto receipt = m_client.buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());

  const auto purchases = m_client.purchases(buyer);
  ASSERT_TRUE(purchases.has_value());
  ASSERT_EQ(purchases->size(), 1U);
  EXPECT_EQ(purchases->front().publication.publication_id, publication_id);
  EXPECT_EQ(purchases->front().publication.title, std::string(k_title));

  const auto content = m_client.fetch_content(buyer, receipt->header.purchase_id);

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(ClientTest, RecoversReceiptOnAnotherDevice) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto receipt = m_client.buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());

  const auto content = m_second_client.fetch_content(buyer, receipt->header.purchase_id);

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);

  const auto stored = m_second_receipts.load(receipt->header.purchase_id);
  ASSERT_TRUE(stored.has_value());
  EXPECT_FALSE(stored->empty());
}

TEST_F(ClientTest, AuthorReceivesOwnPublication) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto author = m_client.log_in("автор");
  ASSERT_TRUE(author.has_value());

  const auto receipt = m_client.buy(author.value(), publication_id);

  ASSERT_TRUE(receipt.has_value());
  EXPECT_EQ(receipt->header.purchase_id, publication_id);

  const auto purchases = m_client.purchases(author.value());
  ASSERT_TRUE(purchases.has_value());
  EXPECT_TRUE(purchases->empty());

  const auto content = m_client.fetch_content(author.value(), publication_id);

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(ClientTest, RefusesCredentialsOfAnotherUser) {
  const std::string text = long_text();
  const auto other = sign_in("другой");
  const auto author = m_client.register_user("автор");
  const auto credentials = m_client.log_in("автор");
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(credentials.has_value());

  const auto keys = generate_author_key();
  const auto identity = content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const Credentials forged{.user_id = other.user_id, .token = credentials->token};
  const PublicationDraft draft{.title = std::string(k_title), .file_name = "файл.txt", .content = text};

  const auto publication = m_client.publish(forged, draft, keys->public_key, signature.value());

  ASSERT_FALSE(publication.has_value());
  EXPECT_EQ(publication.error(), CoreError::authorization_failed);
}

} // namespace
