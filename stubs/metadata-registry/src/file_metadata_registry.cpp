#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <dgds/stubs/support/file_storage.h>

#include <openssl/evp.h>

#include "records.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <system_error>
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

std::optional<std::uint64_t> identifier_of_name(const std::filesystem::path &file) {
  const std::string name = file.stem().string();
  std::uint64_t identifier = 0;
  const char *begin = name.data();
  const char *end = begin + name.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, identifier);

  if (parsed.ec != std::errc() || parsed.ptr != end) {
    return std::nullopt;
  }

  return identifier;
}

template <typename Record>
core::Page<Record> window_of(std::vector<Record> records, std::size_t offset, std::size_t limit) {
  const std::size_t total = records.size();
  std::vector<Record> window;
  window.reserve(std::min(limit, total));

  for (std::size_t index = offset; index < records.size() && window.size() < limit; ++index) {
    window.push_back(std::move(records[index]));
  }

  return core::Page<Record>{.total = total, .records = std::move(window)};
}

template <typename Record, typename Decode>
core::Result<core::Page<Record>> window_of_files(const std::filesystem::path &directory, std::size_t offset,
                                                 std::size_t limit, Decode decode) {
  const auto files = list_files(directory);

  if (!files.has_value()) {
    return std::unexpected(files.error());
  }

  std::vector<std::pair<std::uint64_t, std::filesystem::path>> ordered;
  ordered.reserve(files->size());

  for (const auto &file : files.value()) {
    const auto identifier = identifier_of_name(file);

    if (!identifier.has_value()) {
      return std::unexpected(k_broken_record);
    }

    ordered.emplace_back(identifier.value(), file);
  }

  std::sort(ordered.begin(), ordered.end(),
            [](const auto &left, const auto &right) { return left.first < right.first; });

  const std::size_t total = ordered.size();
  std::vector<Record> records;
  records.reserve(std::min(limit, total));

  for (std::size_t index = offset; index < ordered.size() && records.size() < limit; ++index) {
    const auto data = load_file(ordered[index].second, k_broken_record);

    if (!data.has_value()) {
      return std::unexpected(data.error());
    }

    auto record = decode(data.value());

    if (!record.has_value()) {
      return std::unexpected(k_broken_record);
    }

    records.push_back(std::move(record.value()));
  }

  return core::Page<Record>{.total = total, .records = std::move(records)};
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

core::Result<core::PublicationPage> FileMetadataRegistry::publications(std::size_t offset, std::size_t limit) {
  return window_of_files<core::PublicationRecord>(publications_dir(), offset, limit, decode_publication);
}

core::Result<core::PublicationPage>
FileMetadataRegistry::publications_of_author(const core::UserId &author_id, std::size_t offset, std::size_t limit) {
  const auto all = load_all<core::PublicationRecord>(publications_dir(), decode_publication);

  if (!all.has_value()) {
    return std::unexpected(all.error());
  }

  std::vector<core::PublicationRecord> selected;

  for (auto &publication : all.value()) {
    if (publication.author_id == author_id) {
      selected.push_back(std::move(publication));
    }
  }

  std::sort(selected.begin(), selected.end(),
            [](const core::PublicationRecord &left, const core::PublicationRecord &right) {
              return left.publication_id < right.publication_id;
            });

  return window_of(std::move(selected), offset, limit);
}

core::Result<void> FileMetadataRegistry::add_purchase(const core::PurchaseRecord &purchase) {
  const auto encoded = encode_purchase(purchase);

  if (!encoded.has_value()) {
    return std::unexpected(k_broken_record);
  }

  const std::filesystem::path marker = pair_path(purchase.user_id, purchase.publication_id);
  const auto claimed = store_file_if_absent(marker, std::to_string(purchase.purchase_id));

  if (!claimed.has_value()) {
    return std::unexpected(claimed.error());
  }

  if (!claimed.value()) {
    return std::unexpected(core::CoreError::record_exists);
  }

  const auto stored = store_file_if_absent(purchase_path(purchase.purchase_id), encoded.value());

  if (!stored.has_value()) {
    return std::unexpected(release_marker(marker, stored.error()));
  }

  if (!stored.value()) {
    return std::unexpected(release_marker(marker, core::CoreError::record_exists));
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

core::Result<core::PurchasePage> FileMetadataRegistry::purchases_of_user(const core::UserId &user_id,
                                                                         std::size_t offset, std::size_t limit) {
  const auto all = load_all<core::PurchaseRecord>(purchases_dir(), decode_purchase);

  if (!all.has_value()) {
    return std::unexpected(all.error());
  }

  std::vector<core::PurchaseRecord> selected;

  for (auto &purchase : all.value()) {
    if (purchase.user_id == user_id) {
      selected.push_back(std::move(purchase));
    }
  }

  std::sort(selected.begin(), selected.end(), [](const core::PurchaseRecord &left, const core::PurchaseRecord &right) {
    return left.purchase_id < right.purchase_id;
  });

  return window_of(std::move(selected), offset, limit);
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
