#include <dgds/stubs/key_store/file_key_store.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/core/identity/content_identity.h>

#include <secure_content.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace {

using dgds::core::ContentIdentity;
using dgds::core::CoreError;
using dgds::core::decrypt;
using dgds::core::SymmetricKey;
using dgds::core::unwrap_key;
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

SymmetricKey make_key(std::uint8_t seed) {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key.data()[index] = static_cast<std::uint8_t>(seed + index);
  }

  return key;
}

class KeyStoreTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_root = std::filesystem::temp_directory_path() /
             ("dgds-key-store-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(m_root);
  }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  [[nodiscard]] FileKeyStore make_store() const { return FileKeyStore(m_root / "master.key", m_root / "keys"); }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(KeyStoreTest, OpensSealedContent) {
  auto store = make_store();
  const ContentIdentity identity = make_identity(1);

  const auto sealed = store.seal(identity, make_plaintext(k_plaintext), k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto opened = store.open(identity, sealed.value(), k_associated_data);

  ASSERT_TRUE(opened.has_value());
  EXPECT_EQ(opened->view(), k_plaintext);
}

TEST_F(KeyStoreTest, RejectsSealedContentOfAnotherIdentity) {
  auto store = make_store();
  const ContentIdentity first = make_identity(2);
  const ContentIdentity second = make_identity(3);

  const auto sealed = store.seal(first, make_plaintext(k_plaintext), k_associated_data);
  const auto other = store.seal(second, make_plaintext("чужое содержимое"), k_associated_data);

  ASSERT_TRUE(sealed.has_value());
  ASSERT_TRUE(other.has_value());

  const auto opened = store.open(second, sealed.value(), k_associated_data);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST_F(KeyStoreTest, WrapsTheSameKeyThatSeals) {
  auto store = make_store();
  const ContentIdentity identity = make_identity(4);
  const SymmetricKey purchase_key = make_key(9);

  const auto sealed = store.seal(identity, make_plaintext(k_plaintext), k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto wrapped = store.wrap(identity, purchase_key, k_associated_data);
  ASSERT_TRUE(wrapped.has_value());

  const auto file_key = unwrap_key(wrapped.value(), purchase_key, k_associated_data);
  ASSERT_TRUE(file_key.has_value());

  const auto opened = decrypt(sealed.value(), file_key.value(), k_associated_data);

  ASSERT_TRUE(opened.has_value());
  EXPECT_EQ(opened->view(), k_plaintext);
}

TEST_F(KeyStoreTest, UsesDifferentFileKeysPerIdentity) {
  auto store = make_store();
  const ContentIdentity first = make_identity(5);
  const ContentIdentity second = make_identity(6);
  const SymmetricKey purchase_key = make_key(11);

  const auto first_sealed = store.seal(first, make_plaintext(k_plaintext), k_associated_data);
  const auto second_sealed = store.seal(second, make_plaintext(k_plaintext), k_associated_data);

  ASSERT_TRUE(first_sealed.has_value());
  ASSERT_TRUE(second_sealed.has_value());

  const auto first_wrapped = store.wrap(first, purchase_key, k_associated_data);
  const auto second_wrapped = store.wrap(second, purchase_key, k_associated_data);

  ASSERT_TRUE(first_wrapped.has_value());
  ASSERT_TRUE(second_wrapped.has_value());

  const auto first_key = unwrap_key(first_wrapped.value(), purchase_key, k_associated_data);
  const auto second_key = unwrap_key(second_wrapped.value(), purchase_key, k_associated_data);

  ASSERT_TRUE(first_key.has_value());
  ASSERT_TRUE(second_key.has_value());
  EXPECT_FALSE(first_key->equals(second_key.value()));
}

TEST_F(KeyStoreTest, ReportsMissingFileKey) {
  auto store = make_store();
  const ContentIdentity published = make_identity(7);
  const ContentIdentity unknown = make_identity(8);

  const auto sealed = store.seal(published, make_plaintext(k_plaintext), k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto opened = store.open(unknown, sealed.value(), k_associated_data);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::key_not_found);
}

TEST_F(KeyStoreTest, RefusesToReplaceLostMasterKey) {
  const ContentIdentity identity = make_identity(9);
  const SymmetricKey purchase_key = make_key(13);

  const auto created = make_store().seal(identity, make_plaintext(k_plaintext), k_associated_data);
  ASSERT_TRUE(created.has_value());

  ASSERT_TRUE(std::filesystem::remove(m_root / "master.key"));

  const auto wrapped = make_store().wrap(identity, purchase_key, k_associated_data);

  ASSERT_FALSE(wrapped.has_value());
  EXPECT_EQ(wrapped.error(), CoreError::key_not_found);

  const auto another = make_store().seal(make_identity(10), make_plaintext(k_plaintext), k_associated_data);

  ASSERT_FALSE(another.has_value());
  EXPECT_EQ(another.error(), CoreError::key_not_found);
}

} // namespace
