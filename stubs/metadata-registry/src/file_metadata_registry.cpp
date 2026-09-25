#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <dgds/stubs/support/file_storage.h>

#include <openssl/evp.h>

#include "records.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>
#include <vector>

namespace dgds::stubs {
namespace {

constexpr core::CoreError k_broken_record = core::CoreError::storage_failed;

template <typename Record, typename Decode>
core::Result<std::vector<Record>> load_all(const std::filesystem::path &directory, Decode decode) {
  const auto files = list_files(directory);

  if (!files.has_value()) {
    return std::unexpected(files.error());
  }

  std::vector<Record> records;
  records.reserve(files->size());

  for (const auto &file : files.value()) {
    const auto data = load_file(file, k_broken_record);

    if (!data.has_value()) {
      return std::unexpected(data.error());
    }

    auto record = decode(data.value());

    if (!record.has_value()) {
      return std::unexpected(k_broken_record);
    }

    records.push_back(std::move(record.value()));
  }

  return records;
}

template <typename Record, typename Match> std::optional<Record> pick(const std::vector<Record> &records, Match match) {
  for (const auto &record : records) {
    if (match(record)) {
      return record;
    }
  }

  return std::nullopt;
}

core::CoreError release_marker(const std::filesystem::path &marker, core::CoreError original) {
  const auto released = remove_file(marker);

  return released.has_value() ? original : released.error();
}

} // namespace

core::Result<void> FileMetadataRegistry::add_user(const core::UserAccount &account) {
  const auto marker = name_path(account.name);

  if (!marker.has_value()) {
    return std::unexpected(marker.error());
  }

  const auto claimed = claim_file(marker.value());

  if (!claimed.has_value()) {
    return std::unexpected(claimed.error());
  }

  if (!claimed.value()) {
    return std::unexpected(core::CoreError::user_name_taken);
  }

  const auto encoded = encode_user(account);

  if (!encoded.has_value()) {
    return std::unexpected(release_marker(marker.value(), k_broken_record));
  }

  const auto stored = store_file_if_absent(user_path(account.user_id), encoded.value());

  if (!stored.has_value()) {
    return std::unexpected(release_marker(marker.value(), stored.error()));
  }

  if (!stored.value()) {
    return std::unexpected(release_marker(marker.value(), core::CoreError::record_exists));
  }

  return {};
}

core::Result<std::filesystem::path> FileMetadataRegistry::name_path(core::Content name) const {
  std::array<std::uint8_t, core::k_identity_size> digest{};
  unsigned int length = 0;

  if (EVP_Digest(name.data(), name.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1 ||
      length != digest.size()) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return names_dir() / (core::to_hex(digest.data(), digest.size()) + k_name_suffix);
}

core::Result<core::UserAccount> FileMetadataRegistry::find_user(const core::UserId &user_id) {
  const auto data = load_file(user_path(user_id), core::CoreError::user_not_found);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  auto account = decode_user(data.value());

  if (!account.has_value()) {
    return std::unexpected(k_broken_record);
  }

  return account;
}

core::Result<core::UserAccount> FileMetadataRegistry::find_user_by_name(core::Content name) {
  const auto users = load_all<core::UserAccount>(users_dir(), decode_user);

  if (!users.has_value()) {
    return std::unexpected(users.error());
  }

  auto account = pick(*users, [name](const core::UserAccount &user) { return user.name == name; });

  if (!account.has_value()) {
    return std::unexpected(core::CoreError::user_not_found);
  }

  return std::move(account.value());
}

core::Result<void> FileMetadataRegistry::add_publication(const core::PublicationRecord &publication) {
  const auto encoded = encode_publication(publication);

  if (!encoded.has_value()) {
    return std::unexpected(k_broken_record);
  }

  const auto stored = store_file_if_absent(publication_path(publication.publication_id), encoded.value());

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  if (!stored.value()) {
    return std::unexpected(core::CoreError::record_exists);
  }

  return {};
}

core::Result<core::PublicationRecord>
FileMetadataRegistry::find_publication(const core::PublicationId &publication_id) {
  const auto data = load_file(publication_path(publication_id), core::CoreError::publication_not_found);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  auto publication = decode_publication(data.value());

  if (!publication.has_value()) {
    return std::unexpected(k_broken_record);
  }

  return publication;
}

core::Result<core::PublicationRecords> FileMetadataRegistry::publications() {
  return load_all<core::PublicationRecord>(publications_dir(), decode_publication);
}

core::Result<core::PublicationRecords> FileMetadataRegistry::publications_of_author(const core::UserId &author_id) {
  const auto all = load_all<core::PublicationRecord>(publications_dir(), decode_publication);

  if (!all.has_value()) {
    return std::unexpected(all.error());
  }

  core::PublicationRecords selected;

  for (auto &publication : all.value()) {
    if (publication.author_id == author_id) {
      selected.push_back(std::move(publication));
    }
  }

  return selected;
}

core::Result<void> FileMetadataRegistry::add_purchase(const core::PurchaseRecord &purchase) {
  const auto encoded = encode_purchase(purchase);

  if (!encoded.has_value()) {
    return std::unexpected(k_broken_record);
  }

  const auto stored = store_file_if_absent(purchase_path(purchase.purchase_id), encoded.value());

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  if (!stored.value()) {
    return std::unexpected(core::CoreError::record_exists);
  }

  return {};
}

core::Result<core::PurchaseRecord> FileMetadataRegistry::find_purchase(const core::PurchaseId &purchase_id) {
  const auto data = load_file(purchase_path(purchase_id), core::CoreError::purchase_not_found);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  auto purchase = decode_purchase(data.value());

  if (!purchase.has_value()) {
    return std::unexpected(k_broken_record);
  }

  return purchase;
}

core::Result<core::PurchaseRecord> FileMetadataRegistry::find_purchase_of(const core::UserId &user_id,
                                                                          const core::PublicationId &publication_id) {
  const auto purchases = load_all<core::PurchaseRecord>(purchases_dir(), decode_purchase);

  if (!purchases.has_value()) {
    return std::unexpected(purchases.error());
  }

  auto purchase = pick(*purchases, [&user_id, &publication_id](const core::PurchaseRecord &record) {
    return record.user_id == user_id && record.publication_id == publication_id;
  });

  if (!purchase.has_value()) {
    return std::unexpected(core::CoreError::purchase_not_found);
  }

  return std::move(purchase.value());
}

core::Result<core::PurchaseRecords> FileMetadataRegistry::purchases_of_user(const core::UserId &user_id) {
  const auto all = load_all<core::PurchaseRecord>(purchases_dir(), decode_purchase);

  if (!all.has_value()) {
    return std::unexpected(all.error());
  }

  core::PurchaseRecords selected;

  for (auto &purchase : all.value()) {
    if (purchase.user_id == user_id) {
      selected.push_back(std::move(purchase));
    }
  }

  return selected;
}

core::Result<std::size_t> FileMetadataRegistry::purchase_count(const core::PublicationId &publication_id) {
  const auto purchases = load_all<core::PurchaseRecord>(purchases_dir(), decode_purchase);

  if (!purchases.has_value()) {
    return std::unexpected(purchases.error());
  }

  const auto count =
      std::count_if(purchases->begin(), purchases->end(), [&publication_id](const core::PurchaseRecord &purchase) {
        return purchase.publication_id == publication_id;
      });

  return static_cast<std::size_t>(count);
}

core::Result<void> FileMetadataRegistry::save_receipt(const core::ReceiptRecord &record) {
  const auto encoded = encode_receipt_record(record);

  if (!encoded.has_value()) {
    return std::unexpected(k_broken_record);
  }

  return store_file(receipt_path(record.receipt.header.purchase_id, record.device_key), encoded.value());
}

core::Result<core::ReceiptRecord> FileMetadataRegistry::find_receipt(const core::PurchaseId &purchase_id,
                                                                     const core::DevicePublicKey &device_key) {
  const auto data = load_file(receipt_path(purchase_id, device_key), core::CoreError::receipt_not_found);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  auto record = decode_receipt_record(data.value());

  if (!record.has_value()) {
    return std::unexpected(k_broken_record);
  }

  return record;
}

} // namespace dgds::stubs
