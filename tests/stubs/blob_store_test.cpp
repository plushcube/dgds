#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/key_store/file_key_store.h>

#include <dgds/core/identity/content_identity.h>

#include <secure_content.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

using dgds::core::ContentIdentity;
using dgds::core::CoreError;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileKeyStore;
using dgds::test::make_plaintext;

constexpr std::string_view k_plaintext = "содержимое публикации";
constexpr std::string_view k_associated_data = "ad";

ContentIdentity make_identity(std::uint8_t seed) {
  ContentIdentity identity{};

  for (std::size_t index = 0; index < identity.size(); ++index) {
    identity[index] = static_cast<std::uint8_t>(seed + index);
  }

  return identity;
}

class BlobStoreTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_root = std::filesystem::temp_directory_path() /
             ("dgds-blob-store-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(m_root);
  }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  [[nodiscard]] FileKeyStore make_keys() const { return FileKeyStore(m_root / "master.key", m_root / "keys"); }

  [[nodiscard]] FileBlobStore make_blobs() const { return FileBlobStore(m_root / "blobs"); }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(BlobStoreTest, StoresAndLoadsSealedContent) {
  auto keys = make_keys();
  auto blobs = make_blobs();
  const ContentIdentity identity = make_identity(1);

  const auto sealed = keys.seal(identity, make_plaintext(k_plaintext), k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto stored = blobs.store(identity, sealed.value());
  ASSERT_TRUE(stored.has_value());

  const auto loaded = blobs.load(identity);
  ASSERT_TRUE(loaded.has_value());

  const auto opened = keys.open(identity, loaded.value(), k_associated_data);

  ASSERT_TRUE(opened.has_value());
  EXPECT_EQ(opened->view(), k_plaintext);
}

TEST_F(BlobStoreTest, ReportsMissingBlob) {
  auto blobs = make_blobs();

  const auto loaded = blobs.load(make_identity(2));

  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error(), CoreError::blob_not_found);
}

TEST_F(BlobStoreTest, RejectsMalformedBlob) {
  auto blobs = make_blobs();
  const ContentIdentity identity = make_identity(3);

  const std::filesystem::path root = m_root / "blobs";
  std::filesystem::create_directories(root);

  std::ofstream broken(root / (dgds::core::to_hex(identity) + ".blob"), std::ios::binary);
  broken << "не зашифрованный блоб";
  broken.close();

  const auto loaded = blobs.load(identity);

  ASSERT_FALSE(loaded.has_value());
  EXPECT_EQ(loaded.error(), CoreError::blob_malformed);
}

} // namespace
