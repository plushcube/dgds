#include <dgds/server/services/attribution_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/identifier.h>
#include <dgds/core/models/mark.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/core/watermark/mark_channel.h>
#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/identity_registry/file_identity_registry.h>
#include <dgds/stubs/key_store/file_key_store.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using dgds::core::AuthorPrivateKey;
using dgds::core::Content;
using dgds::core::CoreError;
using dgds::core::embed_mark;
using dgds::core::k_mark_version;
using dgds::core::Mark;
using dgds::core::PublicationDraft;
using dgds::core::PublicationRecord;
using dgds::core::Result;
using dgds::core::sign_author;
using dgds::core::Signature;
using dgds::core::UserAccount;
using dgds::server::AccessKind;
using dgds::server::AttributionService;
using dgds::server::PublicationService;
using dgds::server::PurchaseService;
using dgds::server::UserService;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileIdentityRegistry;
using dgds::stubs::FileKeyStore;
using dgds::stubs::FileMetadataRegistry;

constexpr std::string_view k_title = "название";
constexpr std::int64_t k_published_at = 1700000000;
constexpr std::int64_t k_purchased_at = 1700000500;

Result<Signature> sign_content(Content text, Content author_name, const AuthorPrivateKey &key) {
  const auto identity = dgds::core::content_identity(text);

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  return sign_author(identity.value(), author_name, key);
}

std::string long_text() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += "line " + std::to_string(line) + " of the published text\n";
  }

  return text;
}

class AttributionServiceTest : public ::testing::Test {
protected:
  AttributionServiceTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-attribution-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] UserAccount register_user(std::string_view name) {
    const auto account = m_users.register_user(name);
    EXPECT_TRUE(account.has_value());

    return account.value();
  }

  [[nodiscard]] PublicationRecord publish(const UserAccount &author, std::string_view text) {
    const auto keys = dgds::core::generate_author_key();
    EXPECT_TRUE(keys.has_value());

    const auto signature = sign_content(text, author.name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    const PublicationDraft draft{.title = std::string(k_title), .file_name = "файл.txt", .content = std::string(text)};
    const auto publication =
        m_publications.publish(author.user_id, draft, keys->public_key, signature.value(), k_published_at);
    EXPECT_TRUE(publication.has_value());

    return publication.value();
  }

  [[nodiscard]] std::string leaked_copy(std::string_view text, std::uint64_t context_id) {
    const auto marked = embed_mark(text, Mark{.purchase_id = context_id, .version = k_mark_version});
    EXPECT_TRUE(marked.has_value());

    return marked.value_or(std::string(text));
  }

  std::filesystem::path m_root;
  FileIdentityRegistry m_identities{m_root / "identities"};
  FileKeyStore m_keys{m_root / "master.key", m_root / "keys"};
  FileBlobStore m_blobs{m_root / "blobs"};
  FileMetadataRegistry m_metadata{m_root / "metadata"};
  UserService m_users{m_metadata, m_sessions};
  PublicationService m_publications{m_identities, m_keys, m_blobs, m_metadata};
  PurchaseService m_purchases{m_keys, m_metadata};
  AttributionService m_attribution{m_metadata};

private:
  dgds::server::SessionStore m_sessions;
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(AttributionServiceTest, MatchesPurchaseByMark) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const std::string text = long_text();
  const auto publication = publish(author, text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(receipt.has_value());

  const auto report = m_attribution.attribute(leaked_copy(text, receipt->header.purchase_id));

  ASSERT_TRUE(report.has_value());
  EXPECT_EQ(report->kind, AccessKind::purchase);
  EXPECT_EQ(report->context_id, receipt->header.purchase_id);
  EXPECT_EQ(report->user_id, buyer.user_id);
  EXPECT_EQ(report->user_name, buyer.name);
  EXPECT_EQ(report->publication_id, publication.publication_id);
  EXPECT_EQ(report->title, publication.title);
  EXPECT_EQ(report->granted_at, k_purchased_at);
}

TEST_F(AttributionServiceTest, AttributesAuthorCopy) {
  const UserAccount author = register_user("автор");
  const std::string text = long_text();
  const auto publication = publish(author, text);

  const auto report = m_attribution.attribute(leaked_copy(text, publication.publication_id));

  ASSERT_TRUE(report.has_value());
  EXPECT_EQ(report->kind, AccessKind::author);
  EXPECT_EQ(report->context_id, publication.publication_id);
  EXPECT_EQ(report->user_id, author.user_id);
  EXPECT_EQ(report->user_name, author.name);
  EXPECT_EQ(report->publication_id, publication.publication_id);
  EXPECT_EQ(report->title, publication.title);
  EXPECT_EQ(report->granted_at, k_published_at);
}

TEST_F(AttributionServiceTest, ReportsUnknownMark) {
  const std::string text = long_text();
  const auto unknown = dgds::core::generate_identifier();
  ASSERT_TRUE(unknown.has_value());

  const auto report = m_attribution.attribute(leaked_copy(text, unknown.value()));

  ASSERT_FALSE(report.has_value());
  EXPECT_EQ(report.error(), CoreError::purchase_not_found);
}

TEST_F(AttributionServiceTest, RefusesTextWithoutMark) {
  const std::string text = long_text();

  const auto report = m_attribution.attribute(text);

  ASSERT_FALSE(report.has_value());
  EXPECT_EQ(report.error(), CoreError::mark_not_found);
}

} // namespace
