#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/package.h>
#include <dgds/core/envelope/receipt.h>
#include <dgds/core/identity/canonical_form.h>
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
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using dgds::core::AuthorPrivateKey;
using dgds::core::canonical_form;
using dgds::core::Content;
using dgds::core::CoreError;
using dgds::core::DeviceKeyPair;
using dgds::core::k_package_version;
using dgds::core::open_package;
using dgds::core::open_receipt_key;
using dgds::core::PublicationDraft;
using dgds::core::PublicationRecord;
using dgds::core::read_mark;
using dgds::core::Receipt;
using dgds::core::Result;
using dgds::core::sign_author;
using dgds::core::Signature;
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
constexpr std::string_view k_short_text = "короткий текст";
constexpr std::string_view k_title = "название";
constexpr std::int64_t k_purchased_at = 1700000500;

Result<Signature> sign_content(Content text, Content author_name, const AuthorPrivateKey &key) {
  const auto identity = dgds::core::content_identity(text);

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  return sign_author(identity.value(), author_name, key);
}

std::string markable_text() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += "line " + std::to_string(line) + " of the published text\n";
  }

  return text;
}

class DeliveryServiceTest : public ::testing::Test {
protected:
  DeliveryServiceTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-delivery-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
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
  DeliveryService m_delivery{m_blobs, m_keys, m_metadata};

private:
  dgds::server::SessionStore m_sessions;
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(DeliveryServiceTest, DeliversPackageToOwner) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(receipt.has_value());

  const auto package = m_delivery.fetch_package(buyer.user_id, receipt->header.purchase_id, device->public_key);

  ASSERT_TRUE(package.has_value());
  EXPECT_EQ(package->version, k_package_version);
  EXPECT_EQ(package->identity, publication.identity);
  EXPECT_EQ(package->author_key, publication.author_key);
  EXPECT_EQ(package->author_name, author.name);
  EXPECT_EQ(package->signature, publication.signature);

  const auto receipt_key = open_receipt_key(receipt.value(), device->private_key);
  ASSERT_TRUE(receipt_key.has_value());

  const auto content = open_package(package.value(), receipt_key.value());

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->view(), k_text);
}

TEST_F(DeliveryServiceTest, RepeatsDeliveryForSamePurchase) {
  const UserAccount author = register_user("автор");
  const UserAccount buyer = register_user("покупатель");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(buyer.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(receipt.has_value());

  const auto first = m_delivery.fetch_package(buyer.user_id, receipt->header.purchase_id, device->public_key);
  const auto second = m_delivery.fetch_package(buyer.user_id, receipt->header.purchase_id, device->public_key);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->wrapped_blob_key.ciphertext, first->wrapped_blob_key.ciphertext);
  EXPECT_EQ(second->wrapped_blob_key.nonce, first->wrapped_blob_key.nonce);
  EXPECT_EQ(second->content.ciphertext, first->content.ciphertext);
  EXPECT_EQ(second->identity, first->identity);
}

TEST_F(DeliveryServiceTest, RejectsMissingPurchase) {
  const UserAccount buyer = register_user("покупатель");

  const auto package = m_delivery.fetch_package(buyer.user_id, 4242, dgds::core::DevicePublicKey{});

  ASSERT_FALSE(package.has_value());
  EXPECT_EQ(package.error(), CoreError::purchase_not_found);
}

TEST_F(DeliveryServiceTest, RejectsForeignPurchase) {
  const UserAccount author = register_user("автор");
  const UserAccount owner = register_user("владелец");
  const UserAccount stranger = register_user("чужой");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(owner.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(receipt.has_value());

  const auto package = m_delivery.fetch_package(stranger.user_id, receipt->header.purchase_id, device->public_key);

  ASSERT_FALSE(package.has_value());
  EXPECT_EQ(package.error(), CoreError::not_permitted);
}

TEST_F(DeliveryServiceTest, RejectsPurchaseOfMissingPublication) {
  const UserAccount buyer = register_user("покупатель");

  const auto added = m_metadata.add_purchase(dgds::core::PurchaseRecord{
      .purchase_id = 777, .user_id = buyer.user_id, .publication_id = 888, .purchased_at = k_purchased_at});
  ASSERT_TRUE(added.has_value());

  const auto package = m_delivery.fetch_package(buyer.user_id, 777, dgds::core::DevicePublicKey{});

  ASSERT_FALSE(package.has_value());
  EXPECT_EQ(package.error(), CoreError::publication_not_found);
}

TEST_F(DeliveryServiceTest, MarksDeliveredContentPerPurchase) {
  const UserAccount author = register_user("автор");
  const UserAccount first = register_user("первый");
  const UserAccount second = register_user("второй");
  const std::string text = markable_text();
  const auto publication = publish(author, text);

  const auto first_device = dgds::core::generate_device_key();
  const auto second_device = dgds::core::generate_device_key();
  ASSERT_TRUE(first_device.has_value());
  ASSERT_TRUE(second_device.has_value());

  const auto first_receipt =
      m_purchases.buy(first.user_id, publication.publication_id, first_device->public_key, k_purchased_at);
  const auto second_receipt =
      m_purchases.buy(second.user_id, publication.publication_id, second_device->public_key, k_purchased_at + 10);
  ASSERT_TRUE(first_receipt.has_value());
  ASSERT_TRUE(second_receipt.has_value());

  const auto first_package =
      m_delivery.fetch_package(first.user_id, first_receipt->header.purchase_id, first_device->public_key);
  const auto second_package =
      m_delivery.fetch_package(second.user_id, second_receipt->header.purchase_id, second_device->public_key);
  ASSERT_TRUE(first_package.has_value());
  ASSERT_TRUE(second_package.has_value());

  EXPECT_EQ(first_package->identity, second_package->identity);
  EXPECT_EQ(first_package->signature, second_package->signature);
  EXPECT_NE(first_package->content.ciphertext, second_package->content.ciphertext);

  const auto repeated =
      m_delivery.fetch_package(first.user_id, first_receipt->header.purchase_id, first_device->public_key);
  ASSERT_TRUE(repeated.has_value());
  EXPECT_NE(repeated->content.nonce, first_package->content.nonce);

  const auto check_delivered = [&text](const dgds::core::Package &package, const dgds::core::Receipt &receipt,
                                       const dgds::core::DeviceKeyPair &device) {
    const auto receipt_key = open_receipt_key(receipt, device.private_key);
    ASSERT_TRUE(receipt_key.has_value());

    const auto content = open_package(package, receipt_key.value());
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(canonical_form(content->view()), text);

    const auto mark = read_mark(content->view());
    ASSERT_TRUE(mark.has_value());
    EXPECT_EQ(mark->purchase_id, receipt.header.purchase_id);
  };

  check_delivered(first_package.value(), first_receipt.value(), first_device.value());
  check_delivered(second_package.value(), second_receipt.value(), second_device.value());

  const auto repeat_key = open_receipt_key(first_receipt.value(), first_device->private_key);
  ASSERT_TRUE(repeat_key.has_value());

  const auto repeated_once = open_package(first_package.value(), repeat_key.value());
  const auto repeated_again = open_package(repeated.value(), repeat_key.value());

  ASSERT_TRUE(repeated_once.has_value());
  ASSERT_TRUE(repeated_again.has_value());
  EXPECT_EQ(repeated_again->view(), repeated_once->view());
}

TEST_F(DeliveryServiceTest, SharesUnmarkedDelivery) {
  const UserAccount author = register_user("автор");
  const UserAccount first = register_user("первый");
  const UserAccount second = register_user("второй");
  const auto publication = publish(author, k_short_text);

  const auto first_device = dgds::core::generate_device_key();
  const auto second_device = dgds::core::generate_device_key();
  ASSERT_TRUE(first_device.has_value());
  ASSERT_TRUE(second_device.has_value());

  const auto first_receipt =
      m_purchases.buy(first.user_id, publication.publication_id, first_device->public_key, k_purchased_at);
  const auto second_receipt =
      m_purchases.buy(second.user_id, publication.publication_id, second_device->public_key, k_purchased_at + 10);
  ASSERT_TRUE(first_receipt.has_value());
  ASSERT_TRUE(second_receipt.has_value());

  const auto first_package =
      m_delivery.fetch_package(first.user_id, first_receipt->header.purchase_id, first_device->public_key);
  const auto second_package =
      m_delivery.fetch_package(second.user_id, second_receipt->header.purchase_id, second_device->public_key);
  ASSERT_TRUE(first_package.has_value());
  ASSERT_TRUE(second_package.has_value());

  EXPECT_EQ(first_package->content.ciphertext, second_package->content.ciphertext);
  EXPECT_EQ(first_package->content.nonce, second_package->content.nonce);

  const auto receipt_key = open_receipt_key(first_receipt.value(), first_device->private_key);
  ASSERT_TRUE(receipt_key.has_value());

  const auto content = open_package(first_package.value(), receipt_key.value());

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(content->view(), k_short_text);
  EXPECT_FALSE(read_mark(content->view()).has_value());
}

TEST_F(DeliveryServiceTest, DeliversMarkedCopyToAuthor) {
  const UserAccount author = register_user("автор");
  const auto publication = publish(author, markable_text());

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto receipt = m_purchases.buy(author.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_TRUE(receipt.has_value());

  const auto package = m_delivery.fetch_package(author.user_id, publication.publication_id, device->public_key);
  ASSERT_TRUE(package.has_value());
  EXPECT_EQ(package->identity, publication.identity);

  const auto receipt_key = open_receipt_key(receipt.value(), device->private_key);
  ASSERT_TRUE(receipt_key.has_value());

  const auto content = open_package(package.value(), receipt_key.value());
  ASSERT_TRUE(content.has_value());

  const auto mark = read_mark(content->view());
  ASSERT_TRUE(mark.has_value());
  EXPECT_EQ(mark->purchase_id, publication.publication_id);

  const auto other_device = dgds::core::generate_device_key();
  ASSERT_TRUE(other_device.has_value());

  const auto restored = m_purchases.restore_receipt(author.user_id, publication.publication_id,
                                                    other_device->public_key, k_purchased_at + 10);
  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->header.purchase_id, publication.publication_id);
  EXPECT_EQ(restored->header.purchased_at, 1700000000);
  EXPECT_EQ(restored->header.issued_at, k_purchased_at + 10);

  const auto second_package =
      m_delivery.fetch_package(author.user_id, publication.publication_id, other_device->public_key);
  ASSERT_TRUE(second_package.has_value());

  const auto second_key = open_receipt_key(restored.value(), other_device->private_key);
  ASSERT_TRUE(second_key.has_value());

  const auto second_content = open_package(second_package.value(), second_key.value());
  ASSERT_TRUE(second_content.has_value());
  EXPECT_EQ(second_content->view(), content->view());
}

TEST_F(DeliveryServiceTest, RejectsForeignAccessByPublicationId) {
  const UserAccount author = register_user("автор");
  const UserAccount stranger = register_user("третий");
  const auto publication = publish(author, k_text);

  const auto device = dgds::core::generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto fetched = m_delivery.fetch_package(stranger.user_id, publication.publication_id, device->public_key);
  ASSERT_FALSE(fetched.has_value());
  EXPECT_EQ(fetched.error(), CoreError::not_permitted);

  const auto restored =
      m_purchases.restore_receipt(stranger.user_id, publication.publication_id, device->public_key, k_purchased_at);
  ASSERT_FALSE(restored.has_value());
  EXPECT_EQ(restored.error(), CoreError::not_permitted);

  const auto missing = dgds::core::generate_identifier();
  ASSERT_TRUE(missing.has_value());

  const auto unknown = m_delivery.fetch_package(stranger.user_id, missing.value(), device->public_key);
  ASSERT_FALSE(unknown.has_value());
  EXPECT_EQ(unknown.error(), CoreError::purchase_not_found);
}

} // namespace
