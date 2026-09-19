#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/key_store/file_key_store.h>

#include <dgds/core/crypto/sealed_content_codec.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/core/identity/content_identity.h>

#include <secure_content.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

using dgds::core::as_content;
using dgds::core::Content;
using dgds::core::ContentBuffer;
using dgds::core::ContentIdentity;
using dgds::core::decode_sealed_content;
using dgds::core::SymmetricKey;
using dgds::core::unwrap_key;
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

SymmetricKey make_master_key() {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key.data()[index] = static_cast<std::uint8_t>(index * 7 + 1);
  }

  return key;
}

ContentBuffer read_bytes(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);

  return ContentBuffer(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::vector<std::filesystem::path> files_in(const std::filesystem::path &root) {
  std::vector<std::filesystem::path> files;

  if (!std::filesystem::exists(root)) {
    return files;
  }

  for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }

  return files;
}

bool holds(const std::filesystem::path &root, Content needle) {
  for (const auto &file : files_in(root)) {
    if (read_bytes(file).find(needle) != ContentBuffer::npos) {
      return true;
    }
  }

  return false;
}

class StoreSeparationTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_root = std::filesystem::temp_directory_path() /
             ("dgds-store-separation-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(m_root);

    const SymmetricKey master = make_master_key();
    std::ofstream file(m_root / "master.key", std::ios::binary);
    file.write(reinterpret_cast<const char *>(master.data()), static_cast<std::streamsize>(master.size()));
    file.close();
  }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  [[nodiscard]] std::filesystem::path master_key_path() const { return m_root / "master.key"; }
  [[nodiscard]] std::filesystem::path key_root() const { return m_root / "keys"; }
  [[nodiscard]] std::filesystem::path blob_root() const { return m_root / "blobs"; }

  [[nodiscard]] FileKeyStore make_keys() const { return FileKeyStore(master_key_path(), key_root()); }
  [[nodiscard]] FileBlobStore make_blobs() const { return FileBlobStore(blob_root()); }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(StoreSeparationTest, KeepsFileKeyOutOfContentStore) {
  auto keys = make_keys();
  auto blobs = make_blobs();
  const ContentIdentity identity = make_identity(1);

  const auto sealed = keys.seal(identity, make_plaintext(k_plaintext), k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto stored = blobs.store(identity, sealed.value());
  ASSERT_TRUE(stored.has_value());

  ASSERT_EQ(files_in(blob_root()).size(), 1U);

  const SymmetricKey master = make_master_key();
  const auto wrapped = decode_sealed_content(read_bytes(key_root() / (dgds::core::to_hex(identity) + ".key")));
  ASSERT_TRUE(wrapped.has_value());

  const auto file_key = unwrap_key(wrapped.value(), master, as_content(identity));
  ASSERT_TRUE(file_key.has_value());

  const Content key_bytes(reinterpret_cast<const char *>(file_key->data()), file_key->size());
  const Content nonce_bytes(reinterpret_cast<const char *>(sealed->nonce.data()), sealed->nonce.size());

  EXPECT_TRUE(holds(blob_root(), nonce_bytes));
  EXPECT_FALSE(holds(blob_root(), key_bytes));
  EXPECT_FALSE(holds(blob_root(), k_plaintext));
}

TEST_F(StoreSeparationTest, RequiresKeyStoreToOpenContent) {
  auto blobs = make_blobs();
  const ContentIdentity identity = make_identity(2);

  const auto sealed = make_keys().seal(identity, make_plaintext(k_plaintext), k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  ASSERT_TRUE(blobs.store(identity, sealed.value()).has_value());

  const auto loaded = blobs.load(identity);
  ASSERT_TRUE(loaded.has_value());

  const auto opened = FileKeyStore(m_root / "another-master.key", m_root / "another-keys")
                          .open(identity, loaded.value(), k_associated_data);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), dgds::core::CoreError::key_not_found);
}

} // namespace
