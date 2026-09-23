#pragma once

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
#include <dgds/core/models/author.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/signature/author_signature.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

namespace dgds::test {

inline constexpr std::string_view k_client_title = "название";
inline constexpr std::string_view k_client_line = "строка публикуемого текста для проверки\n";

inline std::string long_text() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += std::string(k_client_line);
  }

  return text;
}

class ClientFixture : public ::testing::Test {
protected:
  ClientFixture() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-client-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] core::Credentials sign_in(std::string_view name) {
    const auto account = m_client.register_user(name);
    EXPECT_TRUE(account.has_value());

    const auto credentials = m_client.log_in(name);
    EXPECT_TRUE(credentials.has_value());

    return credentials.value_or(core::Credentials{});
  }

  [[nodiscard]] core::PublicationId publish(std::string_view name, std::string_view text) {
    const auto credentials = sign_in(name);
    const auto keys = core::generate_author_key();
    EXPECT_TRUE(keys.has_value());

    const auto identity = core::content_identity(text);
    EXPECT_TRUE(identity.has_value());

    const auto signature = core::sign_author(identity.value(), name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    const core::PublicationDraft draft{
        .title = std::string(k_client_title), .file_name = "файл.txt", .content = std::string(text)};
    const auto publication = m_client.publish(credentials, draft, keys->public_key, signature.value());
    EXPECT_TRUE(publication.has_value());

    return publication.value_or(0);
  }

  std::filesystem::path m_root;
  stubs::FileIdentityRegistry m_identities{m_root / "identities"};
  stubs::FileKeyStore m_keys{m_root / "master.key", m_root / "keys"};
  stubs::FileBlobStore m_blobs{m_root / "blobs"};
  stubs::FileMetadataRegistry m_metadata{m_root / "metadata"};
  server::SessionStore m_sessions;
  server::UserService m_users{m_metadata, m_sessions};
  server::CatalogService m_catalog{m_metadata};
  server::PublicationService m_publications{m_identities, m_keys, m_blobs, m_metadata};
  server::PurchaseService m_purchases{m_keys, m_metadata};
  server::DeliveryService m_delivery{m_blobs, m_keys, m_metadata};

  core::Timestamp m_now = 1700000000;
  stubs::DirectTransport m_transport{
      m_users, m_sessions, m_catalog, m_publications, m_purchases, m_delivery, [this]() { return m_now; }};

  stubs::FileDeviceKey m_device_key{m_root / "device.key"};
  stubs::FileReceiptStore m_receipts{m_root / "receipts"};
  client::ApiClient m_client{m_transport, m_device_key, m_receipts};

  stubs::FileDeviceKey m_second_key{m_root / "second.key"};
  stubs::FileReceiptStore m_second_receipts{m_root / "second-receipts"};
  client::ApiClient m_second_client{m_transport, m_second_key, m_second_receipts};

private:
  static inline std::atomic<unsigned> counter{0};
};

} // namespace dgds::test
