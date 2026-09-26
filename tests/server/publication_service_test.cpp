#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/protocol.h>
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
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using dgds::core::as_content;
using dgds::core::AuthorKeyPair;
using dgds::core::AuthorPrivateKey;
using dgds::core::Content;
using dgds::core::content_identity;
using dgds::core::CoreError;
using dgds::core::generate_author_key;
using dgds::core::PublicationDraft;
using dgds::core::Result;
using dgds::core::sign_author;
using dgds::core::Signature;
using dgds::core::UserAccount;
using dgds::server::PublicationService;
using dgds::server::UserService;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileIdentityRegistry;
using dgds::stubs::FileKeyStore;
using dgds::stubs::FileMetadataRegistry;

constexpr std::string_view k_text = "текст публикации";
constexpr std::string_view k_title = "Первое название";
constexpr std::string_view k_file_name = "публикация.txt";
constexpr std::int64_t k_published_at = 1700000000;

Result<Signature> sign_content(Content text, Content author_name, const AuthorPrivateKey &key) {
  const auto identity = content_identity(text);

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  return sign_author(identity.value(), author_name, key);
}

class PublicationServiceTest : public ::testing::Test {
protected:
  PublicationServiceTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-publication-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] UserAccount register_author(std::string_view name) {
    const auto account = m_users.register_user(name);
    EXPECT_TRUE(account.has_value());

    return account.value();
  }

  std::filesystem::path m_root;
  FileIdentityRegistry m_identities{m_root / "identities"};
  FileKeyStore m_keys{m_root / "master.key", m_root / "keys"};
  FileBlobStore m_blobs{m_root / "blobs"};
  FileMetadataRegistry m_metadata{m_root / "metadata"};
  UserService m_users{m_metadata, m_sessions};
  PublicationService m_publications{m_identities, m_keys, m_blobs, m_metadata};

private:
  dgds::server::SessionStore m_sessions;
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(PublicationServiceTest, ReleasesClaimedIdentityWhenPublicationFails) {
  const UserAccount author = register_author("автор");

  const auto keys = generate_author_key();
  ASSERT_TRUE(keys.has_value());

  const auto signature = sign_content(k_text, author.name, keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const PublicationDraft draft{
      .title = std::string(k_title), .file_name = std::string(k_file_name), .content = std::string(k_text)};

  std::filesystem::remove_all(m_root / "blobs");
  std::ofstream(m_root / "blobs") << "не каталог";

  const auto broken =
      m_publications.publish(author.user_id, draft, keys->public_key, signature.value(), k_published_at);

  ASSERT_FALSE(broken.has_value());
  EXPECT_EQ(broken.error(), CoreError::storage_failed);

  std::filesystem::remove(m_root / "blobs");

  const auto publication =
      m_publications.publish(author.user_id, draft, keys->public_key, signature.value(), k_published_at);

  ASSERT_TRUE(publication.has_value()) << dgds::core::code_of(publication.error());
  EXPECT_EQ(publication->identity, dgds::core::content_identity(k_text).value());
}

TEST_F(PublicationServiceTest, PublishesContentSealedForDelivery) {
  const UserAccount author = register_author("автор");

  const auto keys = generate_author_key();
  ASSERT_TRUE(keys.has_value());

  const auto signature = sign_content(k_text, author.name, keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const PublicationDraft draft{
      .title = std::string(k_title), .file_name = std::string(k_file_name), .content = std::string(k_text)};

  const auto publication =
      m_publications.publish(author.user_id, draft, keys->public_key, signature.value(), k_published_at);

  ASSERT_TRUE(publication.has_value());
  EXPECT_EQ(publication->author_id, author.user_id);
  EXPECT_EQ(publication->author_name, author.name);
  EXPECT_EQ(publication->title, k_title);
  EXPECT_EQ(publication->file_name, k_file_name);
  EXPECT_EQ(publication->size, k_text.size());
  EXPECT_EQ(publication->published_at, k_published_at);
  EXPECT_EQ(publication->author_key, keys->public_key);
  EXPECT_EQ(publication->signature, signature.value());

  const auto identity = content_identity(k_text);
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(publication->identity, identity.value());

  const auto catalog = m_metadata.publications(0, dgds::core::k_default_page_size);

  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->records.size(), 1U);
  EXPECT_EQ(catalog->records[0].publication_id, publication->publication_id);

  const auto stored = m_blobs.load(identity.value());
  ASSERT_TRUE(stored.has_value());

  const auto content = m_keys.open(identity.value(), stored.value(), as_content(identity.value()));

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->view(), k_text);
}

TEST_F(PublicationServiceTest, RejectsDuplicateWithoutRevealingMetadata) {
  const UserAccount first = register_author("первый");
  const UserAccount second = register_author("второй");

  const auto first_keys = generate_author_key();
  const auto second_keys = generate_author_key();
  ASSERT_TRUE(first_keys.has_value());
  ASSERT_TRUE(second_keys.has_value());

  const auto first_signature = sign_content(k_text, first.name, first_keys->private_key);
  const auto second_signature = sign_content(k_text, second.name, second_keys->private_key);
  ASSERT_TRUE(first_signature.has_value());
  ASSERT_TRUE(second_signature.has_value());

  const PublicationDraft first_draft{
      .title = std::string(k_title), .file_name = std::string(k_file_name), .content = std::string(k_text)};
  const PublicationDraft second_draft{
      .title = "Второе название", .file_name = "чужое.txt", .content = std::string(k_text)};

  ASSERT_TRUE(m_publications
                  .publish(first.user_id, first_draft, first_keys->public_key, first_signature.value(), k_published_at)
                  .has_value());

  const auto repeated = m_publications.publish(second.user_id, second_draft, second_keys->public_key,
                                               second_signature.value(), k_published_at + 1);

  ASSERT_FALSE(repeated.has_value());
  EXPECT_EQ(repeated.error(), CoreError::content_duplicate);

  const auto catalog = m_metadata.publications(0, dgds::core::k_default_page_size);

  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->records.size(), 1U);
  EXPECT_EQ(catalog->records[0].author_id, first.user_id);
  EXPECT_EQ(catalog->records[0].title, k_title);
  EXPECT_EQ(catalog->records[0].file_name, k_file_name);
}

TEST_F(PublicationServiceTest, RejectsForgedSignature) {
  const UserAccount author = register_author("автор");
  const UserAccount stranger = register_author("чужой");

  const auto author_keys = generate_author_key();
  const auto stranger_keys = generate_author_key();
  ASSERT_TRUE(author_keys.has_value());
  ASSERT_TRUE(stranger_keys.has_value());

  const auto forged = sign_content(k_text, author.name, stranger_keys->private_key);
  ASSERT_TRUE(forged.has_value());

  const PublicationDraft draft{
      .title = std::string(k_title), .file_name = std::string(k_file_name), .content = std::string(k_text)};

  const auto rejected =
      m_publications.publish(author.user_id, draft, author_keys->public_key, forged.value(), k_published_at);

  ASSERT_FALSE(rejected.has_value());
  EXPECT_EQ(rejected.error(), CoreError::signature_invalid);

  const auto catalog = m_metadata.publications(0, dgds::core::k_default_page_size);
  ASSERT_TRUE(catalog.has_value());
  EXPECT_TRUE(catalog->records.empty());

  const auto legitimate = sign_content(k_text, author.name, author_keys->private_key);
  ASSERT_TRUE(legitimate.has_value());

  const auto published =
      m_publications.publish(author.user_id, draft, author_keys->public_key, legitimate.value(), k_published_at);

  ASSERT_TRUE(published.has_value());
  EXPECT_EQ(published->size, k_text.size());
}

TEST_F(PublicationServiceTest, KeepsCanonicalSizeAndIdentity) {
  const UserAccount author = register_author("автор");

  const auto keys = generate_author_key();
  ASSERT_TRUE(keys.has_value());

  const std::string marked = std::string("тек") + "\u200B" + std::string("ст публикации");
  const auto signature = sign_content(marked, author.name, keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const PublicationDraft draft{.title = std::string(k_title), .file_name = std::string(k_file_name), .content = marked};

  const auto publication =
      m_publications.publish(author.user_id, draft, keys->public_key, signature.value(), k_published_at);

  ASSERT_TRUE(publication.has_value());

  const auto identity = content_identity(k_text);
  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(publication->identity, identity.value());
  EXPECT_EQ(publication->size, k_text.size());

  const auto stored = m_blobs.load(identity.value());
  ASSERT_TRUE(stored.has_value());

  const auto content = m_keys.open(identity.value(), stored.value(), as_content(identity.value()));

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->view(), k_text);
}

TEST_F(PublicationServiceTest, RejectsUnknownAuthor) {
  const auto keys = generate_author_key();
  ASSERT_TRUE(keys.has_value());

  const auto signature = sign_content(k_text, "никого", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const PublicationDraft draft{
      .title = std::string(k_title), .file_name = std::string(k_file_name), .content = std::string(k_text)};

  const auto rejected =
      m_publications.publish(dgds::core::UserId{}, draft, keys->public_key, signature.value(), k_published_at);

  ASSERT_FALSE(rejected.has_value());
  EXPECT_EQ(rejected.error(), CoreError::user_not_found);
}

} // namespace
