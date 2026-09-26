#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/protocol.h>
#include <dgds/core/signature/author_signature.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <latch>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

using dgds::core::CoreError;
using dgds::core::DeviceEnvelope;
using dgds::core::DevicePublicKey;
using dgds::core::PublicationRecord;
using dgds::core::PurchaseRecord;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::ReceiptRecord;
using dgds::core::SealedContent;
using dgds::core::UserAccount;
using dgds::core::UserId;
using dgds::stubs::FileMetadataRegistry;

UserId make_user_id(std::uint8_t seed) {
  UserId user_id{};

  for (std::size_t index = 0; index < user_id.size(); ++index) {
    user_id[index] = static_cast<std::uint8_t>(seed * 5 + index);
  }

  return user_id;
}

DevicePublicKey make_device_key(std::uint8_t seed) {
  DevicePublicKey device_key{};

  for (std::size_t index = 0; index < device_key.size(); ++index) {
    device_key[index] = static_cast<std::uint8_t>(seed * 3 + index);
  }

  return device_key;
}

UserAccount make_user(std::uint8_t seed, std::string_view name) {
  return UserAccount{.user_id = make_user_id(seed), .name = std::string(name)};
}

SealedContent make_sealed(std::uint8_t seed) {
  SealedContent sealed{};
  sealed.algorithm = dgds::core::k_aead_algorithm;

  for (std::size_t index = 0; index < sealed.nonce.size(); ++index) {
    sealed.nonce[index] = static_cast<std::uint8_t>(seed + index);
  }

  sealed.ciphertext.resize(24);

  for (std::size_t index = 0; index < sealed.ciphertext.size(); ++index) {
    sealed.ciphertext[index] = static_cast<std::uint8_t>(seed * 2 + index);
  }

  for (std::size_t index = 0; index < sealed.tag.size(); ++index) {
    sealed.tag[index] = static_cast<std::uint8_t>(seed * 4 + index);
  }

  return sealed;
}

PublicationRecord make_publication(std::uint64_t publication_id, const UserId &author_id, std::string_view title) {
  PublicationRecord publication{};

  publication.publication_id = publication_id;
  publication.author_id = author_id;
  publication.author_name = "Автор";
  publication.title = std::string(title);
  publication.file_name = "файл.txt";
  publication.size = 4096;
  publication.published_at = 1700000000;
  publication.signature_algorithm = dgds::core::k_signature_algorithm;

  for (std::size_t index = 0; index < publication.identity.size(); ++index) {
    publication.identity[index] = static_cast<std::uint8_t>(publication_id + index);
  }

  for (std::size_t index = 0; index < publication.author_key.size(); ++index) {
    publication.author_key[index] = static_cast<std::uint8_t>(publication_id * 3 + index);
  }

  for (std::size_t index = 0; index < publication.signature.size(); ++index) {
    publication.signature[index] = static_cast<std::uint8_t>(publication_id * 7 + index);
  }

  return publication;
}

PurchaseRecord make_purchase(std::uint64_t purchase_id, const UserId &user_id, std::uint64_t publication_id) {
  return PurchaseRecord{
      .purchase_id = purchase_id, .user_id = user_id, .publication_id = publication_id, .purchased_at = 1700000500};
}

ReceiptRecord make_receipt_record(std::uint64_t purchase_id, const UserId &user_id, std::uint8_t seed) {
  const ReceiptHeader header{.version = dgds::core::k_receipt_version,
                             .purchase_id = purchase_id,
                             .user_id = user_id,
                             .purchased_at = 1700000500,
                             .issued_at = 1700000600};

  const DeviceEnvelope envelope{.ephemeral_key = make_device_key(seed), .wrapped = make_sealed(seed)};

  return ReceiptRecord{.device_key = make_device_key(static_cast<std::uint8_t>(seed + 1)),
                       .wrapped_blob_key = make_sealed(seed),
                       .receipt = Receipt{.header = header, .wrapped_key = envelope}};
}

class MetadataRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_root = std::filesystem::temp_directory_path() /
             ("dgds-metadata-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(m_root);
  }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  [[nodiscard]] FileMetadataRegistry make_registry() const { return FileMetadataRegistry(m_root); }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(MetadataRegistryTest, KeepsUsersAcrossInstances) {
  const UserAccount account = make_user(1, "автор");

  ASSERT_TRUE(make_registry().add_user(account).has_value());

  const auto by_id = make_registry().find_user(account.user_id);
  const auto by_name = make_registry().find_user_by_name("автор");

  ASSERT_TRUE(by_id.has_value());
  ASSERT_TRUE(by_name.has_value());
  EXPECT_EQ(by_id->name, "автор");
  EXPECT_EQ(by_name->user_id, account.user_id);
}

TEST_F(MetadataRegistryTest, RejectsTakenUserName) {
  ASSERT_TRUE(make_registry().add_user(make_user(1, "автор")).has_value());

  const auto repeated = make_registry().add_user(make_user(2, "автор"));

  ASSERT_FALSE(repeated.has_value());
  EXPECT_EQ(repeated.error(), CoreError::user_name_taken);
}

TEST_F(MetadataRegistryTest, RejectsExistingUserId) {
  const UserAccount account = make_user(3, "автор");

  ASSERT_TRUE(make_registry().add_user(account).has_value());

  const auto repeated = make_registry().add_user(make_user(3, "другой"));

  ASSERT_FALSE(repeated.has_value());
  EXPECT_EQ(repeated.error(), CoreError::record_exists);
}

TEST_F(MetadataRegistryTest, ReportsUnknownUser) {
  const auto by_id = make_registry().find_user(make_user_id(4));
  const auto by_name = make_registry().find_user_by_name("никого");

  ASSERT_FALSE(by_id.has_value());
  EXPECT_EQ(by_id.error(), CoreError::user_not_found);

  ASSERT_FALSE(by_name.has_value());
  EXPECT_EQ(by_name.error(), CoreError::user_not_found);
}

TEST_F(MetadataRegistryTest, KeepsPublicationFields) {
  const UserAccount account = make_user(5, "автор");
  const PublicationRecord publication = make_publication(11, account.user_id, "название");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_publication(publication).has_value());

  const auto found = make_registry().find_publication(publication.publication_id);

  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->publication_id, publication.publication_id);
  EXPECT_EQ(found->author_id, publication.author_id);
  EXPECT_EQ(found->author_name, publication.author_name);
  EXPECT_EQ(found->title, publication.title);
  EXPECT_EQ(found->file_name, publication.file_name);
  EXPECT_EQ(found->size, publication.size);
  EXPECT_EQ(found->published_at, publication.published_at);
  EXPECT_EQ(found->identity, publication.identity);
  EXPECT_EQ(found->author_key, publication.author_key);
  EXPECT_EQ(found->signature_algorithm, publication.signature_algorithm);
  EXPECT_EQ(found->signature, publication.signature);
}

TEST_F(MetadataRegistryTest, ListsPublicationsOfAuthorOnly) {
  const UserAccount first = make_user(6, "первый");
  const UserAccount second = make_user(7, "второй");

  ASSERT_TRUE(make_registry().add_user(first).has_value());
  ASSERT_TRUE(make_registry().add_user(second).has_value());
  ASSERT_TRUE(make_registry().add_publication(make_publication(21, first.user_id, "первая")).has_value());
  ASSERT_TRUE(make_registry().add_publication(make_publication(22, second.user_id, "вторая")).has_value());
  ASSERT_TRUE(make_registry().add_publication(make_publication(23, first.user_id, "третья")).has_value());

  const auto catalog = make_registry().publications(0, dgds::core::k_default_page_size);
  const auto own = make_registry().publications_of_author(first.user_id, 0, dgds::core::k_default_page_size);

  ASSERT_TRUE(catalog.has_value());
  ASSERT_TRUE(own.has_value());
  EXPECT_EQ(catalog->total, 3U);
  EXPECT_EQ(catalog->records.size(), 3U);

  ASSERT_EQ(own->records.size(), 2U);
  EXPECT_EQ(own->total, 2U);
  EXPECT_EQ(own->records[0].publication_id, 21U);
  EXPECT_EQ(own->records[1].publication_id, 23U);
}

TEST_F(MetadataRegistryTest, ReturnsRequestedPublicationWindow) {
  const UserAccount account = make_user(21, "автор");

  ASSERT_TRUE(make_registry().add_user(account).has_value());

  for (const std::uint64_t identifier : {9U, 2U, 10U, 4U, 7U}) {
    ASSERT_TRUE(make_registry().add_publication(make_publication(identifier, account.user_id, "запись")).has_value());
  }

  const auto page = make_registry().publications(1, 2);

  ASSERT_TRUE(page.has_value());
  EXPECT_EQ(page->total, 5U);

  ASSERT_EQ(page->records.size(), 2U);
  EXPECT_EQ(page->records[0].publication_id, 4U);
  EXPECT_EQ(page->records[1].publication_id, 7U);
}

TEST_F(MetadataRegistryTest, OrdersPublicationWindowByNumericIdentifier) {
  const UserAccount account = make_user(22, "автор");

  ASSERT_TRUE(make_registry().add_user(account).has_value());

  for (const std::uint64_t identifier : {2U, 9U, 10U}) {
    ASSERT_TRUE(make_registry().add_publication(make_publication(identifier, account.user_id, "запись")).has_value());
  }

  const auto page = make_registry().publications(0, 3);

  ASSERT_TRUE(page.has_value());

  ASSERT_EQ(page->records.size(), 3U);
  EXPECT_EQ(page->records[0].publication_id, 2U);
  EXPECT_EQ(page->records[1].publication_id, 9U);
  EXPECT_EQ(page->records[2].publication_id, 10U);
}

TEST_F(MetadataRegistryTest, WalksPublicationCatalogInWindowsWithoutRepeats) {
  const UserAccount account = make_user(23, "автор");

  ASSERT_TRUE(make_registry().add_user(account).has_value());

  for (const std::uint64_t identifier : {2U, 5U, 9U, 10U, 11U}) {
    ASSERT_TRUE(make_registry().add_publication(make_publication(identifier, account.user_id, "запись")).has_value());
  }

  std::vector<std::uint64_t> walked;

  for (std::size_t offset = 0;; offset += 2) {
    const auto page = make_registry().publications(offset, 2);

    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->total, 5U);

    if (page->records.empty()) {
      break;
    }

    for (const auto &record : page->records) {
      walked.push_back(record.publication_id);
    }
  }

  EXPECT_EQ(walked, (std::vector<std::uint64_t>{2U, 5U, 9U, 10U, 11U}));
}

TEST_F(MetadataRegistryTest, ReturnsEmptyPublicationWindowBeyondLastRecord) {
  const UserAccount account = make_user(24, "автор");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_publication(make_publication(31, account.user_id, "запись")).has_value());
  ASSERT_TRUE(make_registry().add_publication(make_publication(32, account.user_id, "запись")).has_value());

  const auto page = make_registry().publications(5, 2);

  ASSERT_TRUE(page.has_value());
  EXPECT_EQ(page->total, 2U);
  EXPECT_TRUE(page->records.empty());
}

TEST_F(MetadataRegistryTest, ReadsOnlyRequestedPublicationWindow) {
  const UserAccount account = make_user(25, "автор");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_publication(make_publication(2, account.user_id, "вторая")).has_value());
  ASSERT_TRUE(make_registry().add_publication(make_publication(10, account.user_id, "десятая")).has_value());

  const std::filesystem::path broken = m_root / "publications" / "99.publication";
  std::ofstream(broken, std::ios::binary) << "испорченная запись";

  const auto page = make_registry().publications(0, 2);

  ASSERT_TRUE(page.has_value()) << "испорченная запись за окном сломала выборку";
  EXPECT_EQ(page->total, 3U);

  ASSERT_EQ(page->records.size(), 2U);
  EXPECT_EQ(page->records[0].publication_id, 2U);
  EXPECT_EQ(page->records[1].publication_id, 10U);

  const auto reaching = make_registry().publications(2, 2);

  ASSERT_FALSE(reaching.has_value());
  EXPECT_EQ(reaching.error(), CoreError::storage_failed);
}

TEST_F(MetadataRegistryTest, ReturnsAuthorPublicationWindow) {
  const UserAccount account = make_user(26, "автор");
  const UserAccount other = make_user(27, "другой");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_user(other).has_value());

  for (const std::uint64_t identifier : {6U, 3U, 12U, 8U}) {
    ASSERT_TRUE(make_registry().add_publication(make_publication(identifier, account.user_id, "запись")).has_value());
  }

  ASSERT_TRUE(make_registry().add_publication(make_publication(20, other.user_id, "чужая")).has_value());

  const auto page = make_registry().publications_of_author(account.user_id, 1, 2);

  ASSERT_TRUE(page.has_value());
  EXPECT_EQ(page->total, 4U);

  ASSERT_EQ(page->records.size(), 2U);
  EXPECT_EQ(page->records[0].publication_id, 6U);
  EXPECT_EQ(page->records[1].publication_id, 8U);
}

TEST_F(MetadataRegistryTest, ReportsUnknownPublication) {
  const auto found = make_registry().find_publication(99);

  ASSERT_FALSE(found.has_value());
  EXPECT_EQ(found.error(), CoreError::publication_not_found);
}

TEST_F(MetadataRegistryTest, KeepsPurchaseOfUserAndPublication) {
  const UserAccount account = make_user(8, "покупатель");
  const PurchaseRecord purchase = make_purchase(31, account.user_id, 41);

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_purchase(purchase).has_value());

  const auto by_id = make_registry().find_purchase(purchase.purchase_id);
  const auto by_pair = make_registry().find_purchase_of(account.user_id, 41);

  ASSERT_TRUE(by_id.has_value());
  ASSERT_TRUE(by_pair.has_value());
  EXPECT_EQ(by_pair->purchase_id, purchase.purchase_id);
  EXPECT_EQ(by_id->purchased_at, purchase.purchased_at);
  EXPECT_EQ(by_id->user_id, purchase.user_id);
  EXPECT_EQ(by_id->publication_id, purchase.publication_id);
}

TEST_F(MetadataRegistryTest, RejectsSecondPurchaseOfSamePair) {
  const UserAccount account = make_user(12, "покупатель");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(51, account.user_id, 61)).has_value());

  const auto second = make_registry().add_purchase(make_purchase(52, account.user_id, 61));

  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error(), CoreError::record_exists);

  const auto purchases = make_registry().purchases_of_user(account.user_id, 0, dgds::core::k_default_page_size);

  ASSERT_TRUE(purchases.has_value());
  ASSERT_EQ(purchases->records.size(), 1U);
  EXPECT_EQ(purchases->records.front().purchase_id, 51U);
}

TEST_F(MetadataRegistryTest, KeepsPurchasesOfDifferentPublications) {
  const UserAccount account = make_user(13, "покупатель");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(71, account.user_id, 81)).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(72, account.user_id, 82)).has_value());

  const auto purchases = make_registry().purchases_of_user(account.user_id, 0, dgds::core::k_default_page_size);

  ASSERT_TRUE(purchases.has_value());
  EXPECT_EQ(purchases->records.size(), 2U);
  EXPECT_EQ(purchases->total, 2U);
}

TEST_F(MetadataRegistryTest, KeepsPurchasesOfPublicationByDifferentUsers) {
  const UserAccount first = make_user(14, "первый");
  const UserAccount second = make_user(15, "второй");

  ASSERT_TRUE(make_registry().add_user(first).has_value());
  ASSERT_TRUE(make_registry().add_user(second).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(91, first.user_id, 93)).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(92, second.user_id, 93)).has_value());

  const auto purchases = make_registry().purchases_of_user(second.user_id, 0, dgds::core::k_default_page_size);

  ASSERT_TRUE(purchases.has_value());
  ASSERT_EQ(purchases->records.size(), 1U);
  EXPECT_EQ(purchases->records.front().purchase_id, 92U);
}

TEST_F(MetadataRegistryTest, ListsPurchasesOfUserOnly) {
  const UserAccount first = make_user(9, "первый");
  const UserAccount second = make_user(10, "второй");

  ASSERT_TRUE(make_registry().add_user(first).has_value());
  ASSERT_TRUE(make_registry().add_user(second).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(51, first.user_id, 61)).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(52, second.user_id, 61)).has_value());

  const auto own = make_registry().purchases_of_user(first.user_id, 0, dgds::core::k_default_page_size);

  ASSERT_TRUE(own.has_value());
  ASSERT_EQ(own->records.size(), 1U);
  EXPECT_EQ(own->records[0].purchase_id, 51U);
}

TEST_F(MetadataRegistryTest, ReturnsRequestedPurchaseWindow) {
  const UserAccount account = make_user(28, "покупатель");

  ASSERT_TRUE(make_registry().add_user(account).has_value());

  for (const std::uint64_t identifier : {9U, 2U, 10U, 4U, 7U}) {
    ASSERT_TRUE(make_registry().add_purchase(make_purchase(identifier, account.user_id, identifier)).has_value());
  }

  const auto page = make_registry().purchases_of_user(account.user_id, 1, 2);

  ASSERT_TRUE(page.has_value());
  EXPECT_EQ(page->total, 5U);

  ASSERT_EQ(page->records.size(), 2U);
  EXPECT_EQ(page->records[0].purchase_id, 4U);
  EXPECT_EQ(page->records[1].purchase_id, 7U);
}

TEST_F(MetadataRegistryTest, WalksUserPurchasesInWindowsWithoutRepeats) {
  const UserAccount account = make_user(29, "покупатель");
  const UserAccount other = make_user(30, "другой");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_user(other).has_value());

  for (const std::uint64_t identifier : {2U, 9U, 10U}) {
    ASSERT_TRUE(make_registry().add_purchase(make_purchase(identifier, account.user_id, identifier)).has_value());
  }

  ASSERT_TRUE(make_registry().add_purchase(make_purchase(40, other.user_id, 40)).has_value());

  std::vector<std::uint64_t> walked;

  for (std::size_t offset = 0;; offset += 2) {
    const auto page = make_registry().purchases_of_user(account.user_id, offset, 2);

    ASSERT_TRUE(page.has_value());
    EXPECT_EQ(page->total, 3U);

    if (page->records.empty()) {
      break;
    }

    for (const auto &record : page->records) {
      walked.push_back(record.purchase_id);
    }
  }

  EXPECT_EQ(walked, (std::vector<std::uint64_t>{2U, 9U, 10U}));
}

TEST_F(MetadataRegistryTest, ReturnsEmptyPurchaseWindowBeyondLastRecord) {
  const UserAccount account = make_user(31, "покупатель");

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(51, account.user_id, 61)).has_value());

  const auto page = make_registry().purchases_of_user(account.user_id, 3, 2);

  ASSERT_TRUE(page.has_value());
  EXPECT_EQ(page->total, 1U);
  EXPECT_TRUE(page->records.empty());
}

TEST_F(MetadataRegistryTest, CountsPurchasesOfPublication) {
  const UserAccount author = make_user(11, "автор");
  const UserAccount first = make_user(12, "первый");
  const UserAccount second = make_user(13, "второй");

  ASSERT_TRUE(make_registry().add_user(author).has_value());
  ASSERT_TRUE(make_registry().add_user(first).has_value());
  ASSERT_TRUE(make_registry().add_user(second).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(71, first.user_id, 81)).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(72, second.user_id, 81)).has_value());
  ASSERT_TRUE(make_registry().add_purchase(make_purchase(73, first.user_id, 82)).has_value());

  const auto sold = make_registry().purchase_count(81);
  const auto unsold = make_registry().purchase_count(82);
  const auto unknown = make_registry().purchase_count(83);

  ASSERT_TRUE(sold.has_value());
  EXPECT_EQ(sold.value(), 2U);

  ASSERT_TRUE(unsold.has_value());
  EXPECT_EQ(unsold.value(), 1U);

  ASSERT_TRUE(unknown.has_value());
  EXPECT_EQ(unknown.value(), 0U);
}

TEST_F(MetadataRegistryTest, ReportsUnknownPurchase) {
  const auto by_id = make_registry().find_purchase(91);
  const auto by_pair = make_registry().find_purchase_of(make_user_id(14), 92);

  ASSERT_FALSE(by_id.has_value());
  EXPECT_EQ(by_id.error(), CoreError::purchase_not_found);

  ASSERT_FALSE(by_pair.has_value());
  EXPECT_EQ(by_pair.error(), CoreError::purchase_not_found);
}

TEST_F(MetadataRegistryTest, KeepsReceiptOfDevice) {
  const UserAccount account = make_user(15, "покупатель");
  const ReceiptRecord record = make_receipt_record(101, account.user_id, 16);

  ASSERT_TRUE(make_registry().add_user(account).has_value());
  ASSERT_TRUE(make_registry().save_receipt(record).has_value());

  const auto found = make_registry().find_receipt(101, record.device_key);
  const auto foreign = make_registry().find_receipt(101, make_device_key(200));

  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->device_key, record.device_key);
  EXPECT_EQ(found->wrapped_blob_key.nonce, record.wrapped_blob_key.nonce);
  EXPECT_EQ(found->wrapped_blob_key.ciphertext, record.wrapped_blob_key.ciphertext);
  EXPECT_EQ(found->wrapped_blob_key.tag, record.wrapped_blob_key.tag);
  EXPECT_EQ(found->receipt.header, record.receipt.header);
  EXPECT_EQ(found->receipt.wrapped_key.ephemeral_key, record.receipt.wrapped_key.ephemeral_key);
  EXPECT_EQ(found->receipt.wrapped_key.wrapped.nonce, record.receipt.wrapped_key.wrapped.nonce);
  EXPECT_EQ(found->receipt.wrapped_key.wrapped.ciphertext, record.receipt.wrapped_key.wrapped.ciphertext);
  EXPECT_EQ(found->receipt.wrapped_key.wrapped.tag, record.receipt.wrapped_key.wrapped.tag);

  ASSERT_FALSE(foreign.has_value());
  EXPECT_EQ(foreign.error(), CoreError::receipt_not_found);
}

TEST_F(MetadataRegistryTest, OnlyOneConcurrentRegistrationOfNameSucceeds) {
  constexpr std::size_t k_thread_count = 8;

  FileMetadataRegistry registry(m_root);

  std::atomic<std::size_t> added{0};
  std::atomic<std::size_t> rejected{0};
  std::atomic<std::size_t> failed{0};
  std::latch start(k_thread_count);

  std::vector<std::thread> threads;
  threads.reserve(k_thread_count);

  for (std::size_t index = 0; index < k_thread_count; ++index) {
    threads.emplace_back([&registry, &start, &added, &rejected, &failed, index] {
      start.arrive_and_wait();

      const auto outcome = registry.add_user(make_user(static_cast<std::uint8_t>(20 + index), "одно-имя"));

      if (!outcome.has_value()) {
        if (outcome.error() == CoreError::user_name_taken) {
          ++rejected;
        } else {
          ++failed;
        }

        return;
      }

      ++added;
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  EXPECT_EQ(failed.load(), 0U);
  EXPECT_EQ(added.load(), 1U);
  EXPECT_EQ(rejected.load(), k_thread_count - 1);
}

} // namespace
