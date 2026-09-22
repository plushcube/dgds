#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/identity_registry/file_identity_registry.h>
#include <dgds/stubs/key_store/file_key_store.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using dgds::core::AuthorPrivateKey;
using dgds::core::Content;
using dgds::core::PublicationDraft;
using dgds::core::Result;
using dgds::core::sign_author;
using dgds::core::Signature;
using dgds::core::UserAccount;
using dgds::server::CatalogService;
using dgds::server::PublicationService;
using dgds::server::UserService;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileIdentityRegistry;
using dgds::stubs::FileKeyStore;
using dgds::stubs::FileMetadataRegistry;

constexpr std::string_view k_text = "текст публикации";
constexpr std::string_view k_started_at = "начало ";
constexpr std::int64_t k_published_at = 1700000000;

Result<Signature> sign_content(Content text, Content author_name, const AuthorPrivateKey &key) {
  const auto identity = dgds::core::content_identity(text);

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  return sign_author(identity.value(), author_name, key);
}

const dgds::core::AuthorPublicationSummary *find_authored(const dgds::core::AuthorPublicationSummaries &summaries,
                                                          dgds::core::PublicationId publication_id) {
  for (const auto &summary : summaries) {
    if (summary.publication.publication_id == publication_id) {
      return &summary;
    }
  }

  return nullptr;
}

class CatalogServiceTest : public ::testing::Test {
protected:
  CatalogServiceTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-catalog-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] UserAccount register_author(std::string_view name) {
    const auto account = m_users.register_user(name);
    EXPECT_TRUE(account.has_value());

    return account.value();
  }

  [[nodiscard]] dgds::core::PublicationRecord publish(const UserAccount &author, std::string_view text,
                                                      std::string_view title, std::int64_t published_at) {
    const auto keys = dgds::core::generate_author_key();
    EXPECT_TRUE(keys.has_value());

    const auto signature = sign_content(text, author.name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    const PublicationDraft draft{.title = std::string(title), .file_name = "файл.txt", .content = std::string(text)};

    const auto publication =
        m_publications.publish(author.user_id, draft, keys->public_key, signature.value(), published_at);
    EXPECT_TRUE(publication.has_value());

    return publication.value();
  }

  std::filesystem::path m_root;
  FileIdentityRegistry m_identities{m_root / "identities"};
  FileKeyStore m_keys{m_root / "master.key", m_root / "keys"};
  FileBlobStore m_blobs{m_root / "blobs"};
  FileMetadataRegistry m_metadata{m_root / "metadata"};
  UserService m_users{m_metadata, m_sessions};
  PublicationService m_publications{m_identities, m_keys, m_blobs, m_metadata};
  CatalogService m_catalog{m_metadata};

private:
  dgds::server::SessionStore m_sessions;
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(CatalogServiceTest, ListsCatalogWithRequiredFields) {
  const UserAccount author = register_author("автор");
  const std::string marked = std::string(k_started_at) + "\u200B" + std::string(k_text);

  const auto publication = publish(author, marked, "название", k_published_at);

  const auto catalog = m_catalog.catalog();

  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->size(), 1U);
  EXPECT_EQ((*catalog)[0].publication_id, publication.publication_id);
  EXPECT_EQ((*catalog)[0].title, "название");
  EXPECT_EQ((*catalog)[0].file_name, "файл.txt");
  EXPECT_EQ((*catalog)[0].size, k_started_at.size() + k_text.size());
  EXPECT_EQ((*catalog)[0].published_at, k_published_at);
  EXPECT_EQ((*catalog)[0].author_name, author.name);
}

TEST_F(CatalogServiceTest, ListsCatalogWithoutCredentials) {
  const UserAccount author = register_author("автор");
  const auto publication = publish(author, k_text, "название", k_published_at);

  CatalogService anonymous{m_metadata};

  const auto catalog = anonymous.catalog();

  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->size(), 1U);
  EXPECT_EQ((*catalog)[0].publication_id, publication.publication_id);
  EXPECT_EQ((*catalog)[0].title, "название");
}

TEST_F(CatalogServiceTest, ListsOnlyOwnPublications) {
  const UserAccount first = register_author("первый");
  const UserAccount second = register_author("второй");

  const auto first_one = publish(first, "первый текст", "первая", k_published_at);
  const auto first_two = publish(first, "второй текст", "вторая", k_published_at + 1);
  const auto second_one = publish(second, "третий текст", "третья", k_published_at + 2);

  const auto own = m_catalog.author_publications(first.user_id);

  ASSERT_TRUE(own.has_value());
  ASSERT_EQ(own->size(), 2U);
  EXPECT_NE(find_authored(own.value(), first_one.publication_id), nullptr);
  EXPECT_NE(find_authored(own.value(), first_two.publication_id), nullptr);
  EXPECT_EQ(find_authored(own.value(), second_one.publication_id), nullptr);

  const auto other = m_catalog.author_publications(second.user_id);

  ASSERT_TRUE(other.has_value());
  ASSERT_EQ(other->size(), 1U);
  EXPECT_NE(find_authored(other.value(), second_one.publication_id), nullptr);
}

TEST_F(CatalogServiceTest, CountsPurchasesInAuthorList) {
  const UserAccount author = register_author("автор");
  const UserAccount buyer = register_author("покупатель");

  const auto sold = publish(author, k_text, "проданная", k_published_at);
  const auto unsold = publish(author, "другой текст", "непроданная", k_published_at + 1);

  ASSERT_TRUE(m_metadata
                  .add_purchase(dgds::core::PurchaseRecord{.purchase_id = 1,
                                                           .user_id = buyer.user_id,
                                                           .publication_id = sold.publication_id,
                                                           .purchased_at = k_published_at + 10,
                                                           .wrapped_blob_key = dgds::core::SealedContent{}})
                  .has_value());
  ASSERT_TRUE(m_metadata
                  .add_purchase(dgds::core::PurchaseRecord{.purchase_id = 2,
                                                           .user_id = author.user_id,
                                                           .publication_id = sold.publication_id,
                                                           .purchased_at = k_published_at + 11,
                                                           .wrapped_blob_key = dgds::core::SealedContent{}})
                  .has_value());

  const auto own = m_catalog.author_publications(author.user_id);

  ASSERT_TRUE(own.has_value());
  ASSERT_EQ(own->size(), 2U);

  const auto *sold_summary = find_authored(own.value(), sold.publication_id);
  const auto *unsold_summary = find_authored(own.value(), unsold.publication_id);

  ASSERT_NE(sold_summary, nullptr);
  ASSERT_NE(unsold_summary, nullptr);
  EXPECT_EQ(sold_summary->purchases, 2U);
  EXPECT_EQ(unsold_summary->purchases, 0U);
}

} // namespace
