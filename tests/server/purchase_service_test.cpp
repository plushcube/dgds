#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/keys.h>
#include <dgds/core/envelope/package.h>
#include <dgds/core/envelope/receipt.h>
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
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using dgds::core::as_content;
using dgds::core::AuthorPrivateKey;
using dgds::core::Content;
using dgds::core::CoreError;
using dgds::core::decrypt;
using dgds::core::open_package;
using dgds::core::open_receipt_key;
using dgds::core::PublicationDraft;
using dgds::core::PublicationRecord;
using dgds::core::Result;
using dgds::core::sign_author;
using dgds::core::Signature;
using dgds::core::unwrap_key;
using dgds::core::UserAccount;
using dgds::server::DeliveryService;
using dgds::server::PublicationService;
using dgds::server::PurchaseService;
using dgds::server::UserService;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileIdentityRegistry;
using dgds::stubs::FileKeyStore;
using dgds::stubs::FileMetadataRegistry;

constexpr std::string_view k_text = "текст публикации";
constexpr std::string_view k_title = "название";
constexpr std::int64_t k_purchased_at = 1700000500;

Result<Signature> sign_content(Content text, Content author_name, const AuthorPrivateKey &key) {
  const auto identity = dgds::core::content_identity(text);

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  return sign_author(identity.value(), author_name, key);
}

class PurchaseServiceTest : public ::testing::Test {
protected:
  PurchaseServiceTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-purchase-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
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
        m_publications.publish(author.user_id, draft, keys->public_key, signature.value(), 1700000000);
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
  PurchaseService m_purchases{m_keys, m_metadata};
  DeliveryService m_delivery{m_blobs, m_metadata};

private:
  dgds::server::SessionStore m_sessions;
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(PurchaseServiceTest, BuysPublicationAndOpensPackage) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);

  ASSERT_TRUE(receipt.has_value());
  EXPECT_EQ(receipt->header.version, dgds::core::k_receipt_version);
  EXPECT_EQ(receipt->header.user_id, buyer.user_id);
  EXPECT_EQ(receipt->header.purchased_at, k_purchased_at);
  EXPECT_EQ(receipt->header.issued_at, k_purchased_at);

  const auto receipt_key = open_receipt_key(receipt.value(), device->private_key);
  ASSERT_TRUE(receipt_key.has_value());

  const auto record = m_metadata.find_receipt(receipt->header.purchase_id, device->public_key);
  ASSERT_TRUE(record.has_value());

  const auto file_key = unwrap_key(record->wrapped_blob_key, receipt_key.value(), as_content(publication.identity));
  ASSERT_TRUE(file_key.has_value());

  const auto stored = m_blobs.load(publication.identity);
  ASSERT_TRUE(stored.has_value());

  const auto content = decrypt(stored.value(), file_key.value(), as_content(publication.identity));

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->view(), k_text);
}

TEST_F(PurchaseServiceTest, RepeatsPurchaseIdempotently) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const UserAccount other = register_user("другой");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto first = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);
  const auto repeated =
      m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at + 60);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(repeated.has_value());
  EXPECT_EQ(repeated->header.purchase_id, first->header.purchase_id);
  EXPECT_EQ(repeated->header.purchased_at, first->header.purchased_at);
  EXPECT_EQ(repeated->wrapped_key.ephemeral_key, first->wrapped_key.ephemeral_key);
  EXPECT_EQ(repeated->wrapped_key.wrapped.ciphertext, first->wrapped_key.wrapped.ciphertext);

  const auto sold_once = m_metadata.purchase_count(publication.publication_id);

  ASSERT_TRUE(sold_once.has_value());
  EXPECT_EQ(sold_once.value(), 1U);

  const auto second_device = dgds::core::generate_device_key();
  ASSERT_TRUE(second_device.has_value());

  const auto sold_twice =
      m_purchases.buy(other.user_id, publication.publication_id, second_device->public_key, k_purchased_at + 120);

  ASSERT_TRUE(sold_twice.has_value());
  EXPECT_NE(sold_twice->header.purchase_id, first->header.purchase_id);

  const auto count = m_metadata.purchase_count(publication.publication_id);

  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 2U);
}

TEST_F(PurchaseServiceTest, RejectsUnknownPublication) {
  const UserAccount buyer = register_user("покупатель");

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(buyer.user_id, 42, device->public_key, k_purchased_at);

  ASSERT_FALSE(receipt.has_value());
  EXPECT_EQ(receipt.error(), CoreError::publication_not_found);

  const auto purchases = m_metadata.purchases_of_user(buyer.user_id);

  ASSERT_TRUE(purchases.has_value());
  EXPECT_TRUE(purchases->empty());
}

TEST_F(PurchaseServiceTest, RequiresReceiptForRequestingDevice) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  const auto foreign = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());
  ASSERT_TRUE(foreign.has_value());

  const auto first = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(first.has_value());

  const auto repeated =
      m_purchases.buy(buyer.user_id, publication.publication_id, foreign->public_key, k_purchased_at + 60);

  ASSERT_FALSE(repeated.has_value());
  EXPECT_EQ(repeated.error(), CoreError::receipt_not_found);

  const auto count = m_metadata.purchase_count(publication.publication_id);

  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1U);
}

TEST_F(PurchaseServiceTest, ListsPurchasedWithPublicationMetadata) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(receipt.has_value());

  const auto purchased = m_purchases.purchases_of(buyer.user_id);

  ASSERT_TRUE(purchased.has_value());
  ASSERT_EQ(purchased->size(), 1U);
  EXPECT_EQ((*purchased)[0].purchase_id, receipt->header.purchase_id);
  EXPECT_EQ((*purchased)[0].purchased_at, k_purchased_at);
  EXPECT_EQ((*purchased)[0].publication.publication_id, publication.publication_id);
  EXPECT_EQ((*purchased)[0].publication.title, k_title);
  EXPECT_EQ((*purchased)[0].publication.file_name, "файл.txt");
  EXPECT_EQ((*purchased)[0].publication.size, k_text.size());
  EXPECT_EQ((*purchased)[0].publication.published_at, 1700000000);
  EXPECT_EQ((*purchased)[0].publication.author_name, author.name);
}

TEST_F(PurchaseServiceTest, HidesPurchasesOfOthers) {
  const UserAccount author = register_user("автор");
  const UserAccount first = register_user("первый");
  const UserAccount second = register_user("второй");
  const UserAccount stranger = register_user("прохожий");

  const auto first_publication = publish(author, k_text);
  const auto second_publication = publish(author, "другой текст");

  const auto first_device = dgds::core::generate_device_key();
  const auto second_device = dgds::core::generate_device_key();
  ASSERT_TRUE(first_device.has_value());
  ASSERT_TRUE(second_device.has_value());

  const auto first_receipt =
      m_purchases.buy(first.user_id, first_publication.publication_id, first_device->public_key, k_purchased_at);
  const auto second_receipt = m_purchases.buy(second.user_id, second_publication.publication_id,
                                              second_device->public_key, k_purchased_at + 10);

  ASSERT_TRUE(first_receipt.has_value());
  ASSERT_TRUE(second_receipt.has_value());

  const auto own = m_purchases.purchases_of(first.user_id);

  ASSERT_TRUE(own.has_value());
  ASSERT_EQ(own->size(), 1U);
  EXPECT_EQ((*own)[0].purchase_id, first_receipt->header.purchase_id);
  EXPECT_EQ((*own)[0].publication.publication_id, first_publication.publication_id);

  for (const auto &summary : own.value()) {
    EXPECT_NE(summary.purchase_id, second_receipt->header.purchase_id);
    EXPECT_NE(summary.publication.publication_id, second_publication.publication_id);
  }

  const auto stranger_purchases = m_purchases.purchases_of(stranger.user_id);

  ASSERT_TRUE(stranger_purchases.has_value());
  EXPECT_TRUE(stranger_purchases->empty());
}

TEST_F(PurchaseServiceTest, RestoresReceiptForAnotherDevice) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const auto publication = publish(author, k_text);

  const auto first_device = dgds::core::generate_device_key();
  const auto second_device = dgds::core::generate_device_key();
  ASSERT_TRUE(first_device.has_value());
  ASSERT_TRUE(second_device.has_value());

  const auto bought =
      m_purchases.buy(buyer.user_id, publication.publication_id, first_device->public_key, k_purchased_at);
  ASSERT_TRUE(bought.has_value());

  const auto restored = m_purchases.restore_receipt(buyer.user_id, bought->header.purchase_id,
                                                    second_device->public_key, k_purchased_at + 300);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->header.purchase_id, bought->header.purchase_id);
  EXPECT_EQ(restored->header.user_id, buyer.user_id);
  EXPECT_EQ(restored->header.purchased_at, k_purchased_at);
  EXPECT_EQ(restored->header.issued_at, k_purchased_at + 300);
  EXPECT_NE(restored->wrapped_key.ephemeral_key, bought->wrapped_key.ephemeral_key);

  const auto first_record = m_metadata.find_receipt(bought->header.purchase_id, first_device->public_key);
  const auto second_record = m_metadata.find_receipt(bought->header.purchase_id, second_device->public_key);
  ASSERT_TRUE(first_record.has_value());
  ASSERT_TRUE(second_record.has_value());
  EXPECT_NE(second_record->device_key, first_record->device_key);
  EXPECT_NE(second_record->wrapped_blob_key.ciphertext, first_record->wrapped_blob_key.ciphertext);

  const auto count = m_metadata.purchase_count(publication.publication_id);
  ASSERT_TRUE(count.has_value());
  EXPECT_EQ(count.value(), 1U);

  for (const auto &device : {first_device.value(), second_device.value()}) {
    const auto package = m_delivery.fetch_package(buyer.user_id, bought->header.purchase_id, device.public_key);
    ASSERT_TRUE(package.has_value());

    const auto receipt = m_metadata.find_receipt(bought->header.purchase_id, device.public_key);
    ASSERT_TRUE(receipt.has_value());

    const auto receipt_key = open_receipt_key(receipt->receipt, device.private_key);
    ASSERT_TRUE(receipt_key.has_value());

    const auto content = open_package(package.value(), receipt_key.value());
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content->view(), k_text);
  }
}

TEST_F(PurchaseServiceTest, RefreshesReceiptForSameDevice) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto bought = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(bought.has_value());

  const auto refreshed =
      m_purchases.restore_receipt(buyer.user_id, bought->header.purchase_id, device->public_key, k_purchased_at + 600);

  ASSERT_TRUE(refreshed.has_value());
  EXPECT_EQ(refreshed->header.purchased_at, k_purchased_at);
  EXPECT_EQ(refreshed->header.issued_at, k_purchased_at + 600);

  const auto package = m_delivery.fetch_package(buyer.user_id, bought->header.purchase_id, device->public_key);
  ASSERT_TRUE(package.has_value());

  const auto receipt_key = open_receipt_key(refreshed.value(), device->private_key);
  ASSERT_TRUE(receipt_key.has_value());

  const auto content = open_package(package.value(), receipt_key.value());

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->view(), k_text);
}

TEST_F(PurchaseServiceTest, RejectsRestoreOfForeignPurchase) {
  const UserAccount author = register_user("автор");
  const UserAccount owner = register_user("владелец");
  const UserAccount stranger = register_user("чужой");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto bought = m_purchases.buy(owner.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(bought.has_value());

  const auto foreign = m_purchases.restore_receipt(stranger.user_id, bought->header.purchase_id, device->public_key,
                                                   k_purchased_at + 900);

  ASSERT_FALSE(foreign.has_value());
  EXPECT_EQ(foreign.error(), CoreError::not_permitted);

  const auto missing = m_purchases.restore_receipt(owner.user_id, 5150, device->public_key, k_purchased_at + 900);

  ASSERT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error(), CoreError::purchase_not_found);
}

} // namespace
