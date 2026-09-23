#pragma once

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/ports/metadata_registry.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>

namespace dgds::stubs {

class FileMetadataRegistry : public core::MetadataRegistry {
public:
  explicit FileMetadataRegistry(std::filesystem::path root) : m_root(std::move(root)) {}

  [[nodiscard]] core::Result<void> add_user(const core::UserAccount &account) override;
  [[nodiscard]] core::Result<core::UserAccount> find_user(const core::UserId &user_id) override;
  [[nodiscard]] core::Result<core::UserAccount> find_user_by_name(core::Content name) override;

  [[nodiscard]] core::Result<void> add_publication(const core::PublicationRecord &publication) override;
  [[nodiscard]] core::Result<core::PublicationRecord>
  find_publication(const core::PublicationId &publication_id) override;
  [[nodiscard]] core::Result<core::PublicationRecords> publications() override;
  [[nodiscard]] core::Result<core::PublicationRecords> publications_of_author(const core::UserId &author_id) override;

  [[nodiscard]] core::Result<void> add_purchase(const core::PurchaseRecord &purchase) override;
  [[nodiscard]] core::Result<core::PurchaseRecord> find_purchase(const core::PurchaseId &purchase_id) override;
  [[nodiscard]] core::Result<core::PurchaseRecord> find_purchase_of(const core::UserId &user_id,
                                                                    const core::PublicationId &publication_id) override;
  [[nodiscard]] core::Result<core::PurchaseRecords> purchases_of_user(const core::UserId &user_id) override;
  [[nodiscard]] core::Result<std::size_t> purchase_count(const core::PublicationId &publication_id) override;

  [[nodiscard]] core::Result<void> save_receipt(const core::ReceiptRecord &record) override;
  [[nodiscard]] core::Result<core::ReceiptRecord> find_receipt(const core::PurchaseId &purchase_id,
                                                               const core::DevicePublicKey &device_key) override;

private:
  [[nodiscard]] std::filesystem::path users_dir() const { return m_root / k_users_directory; }
  [[nodiscard]] std::filesystem::path names_dir() const { return m_root / k_names_directory; }
  [[nodiscard]] std::filesystem::path publications_dir() const { return m_root / k_publications_directory; }
  [[nodiscard]] std::filesystem::path purchases_dir() const { return m_root / k_purchases_directory; }

  [[nodiscard]] core::Result<std::filesystem::path> name_path(core::Content name) const;

  [[nodiscard]] std::filesystem::path user_path(const core::UserId &user_id) const {
    return users_dir() / (core::to_hex(user_id.data(), user_id.size()) + k_user_suffix);
  }

  [[nodiscard]] std::filesystem::path publication_path(const core::PublicationId &publication_id) const {
    return publications_dir() / (std::to_string(publication_id) + k_publication_suffix);
  }

  [[nodiscard]] std::filesystem::path purchase_path(const core::PurchaseId &purchase_id) const {
    return purchases_dir() / (std::to_string(purchase_id) + k_purchase_suffix);
  }

  [[nodiscard]] std::filesystem::path receipt_path(const core::PurchaseId &purchase_id,
                                                   const core::DevicePublicKey &device_key) const {
    return m_root / k_receipts_directory /
           (std::to_string(purchase_id) + "-" + core::to_hex(device_key.data(), device_key.size()) + k_receipt_suffix);
  }

  static constexpr const char *k_users_directory = "users";
  static constexpr const char *k_names_directory = "names";
  static constexpr const char *k_name_suffix = ".name";
  static constexpr const char *k_publications_directory = "publications";
  static constexpr const char *k_purchases_directory = "purchases";
  static constexpr const char *k_receipts_directory = "receipts";
  static constexpr const char *k_user_suffix = ".user";
  static constexpr const char *k_publication_suffix = ".publication";
  static constexpr const char *k_purchase_suffix = ".purchase";
  static constexpr const char *k_receipt_suffix = ".receipt";

  std::filesystem::path m_root;
};

} // namespace dgds::stubs
