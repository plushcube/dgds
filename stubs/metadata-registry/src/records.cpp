#include "records.h"

#include <dgds/core/codec/binary.h>
#include <dgds/core/crypto/sealed_content_codec.h>
#include <dgds/core/envelope/receipt.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <utility>

namespace dgds::stubs {
namespace {

constexpr core::CoreError k_broken_record = core::CoreError::storage_failed;

void append_text(core::ContentBuffer &data, core::Content text) {
  core::append_integer(data, text.size(), core::k_length_size);
  core::append_bytes(data, reinterpret_cast<const std::uint8_t *>(text.data()), text.size());
}

bool read_text(core::Reader &reader, core::ContentBuffer &text) {
  std::uint64_t size = 0;

  if (!reader.read_integer(size, core::k_length_size) || size > reader.remaining()) {
    return false;
  }

  text.resize(static_cast<std::size_t>(size));

  return reader.read_bytes(reinterpret_cast<std::uint8_t *>(text.data()), text.size());
}

core::Result<core::ContentBuffer> append_sealed(core::ContentBuffer &data, const core::SealedContent &sealed) {
  const auto encoded = core::encode_sealed_content(sealed);

  if (!encoded.has_value()) {
    return std::unexpected(k_broken_record);
  }

  data.append(encoded.value());

  return data;
}

} // namespace

core::Result<core::ContentBuffer> encode_user(const core::UserAccount &account) {
  core::ContentBuffer data;

  core::append_bytes(data, account.user_id.data(), account.user_id.size());
  core::append_bytes(data, reinterpret_cast<const std::uint8_t *>(account.name.data()), account.name.size());

  return data;
}

core::Result<core::UserAccount> decode_user(core::Content data) {
  core::Reader reader(data);

  core::UserAccount account{};

  if (!reader.read_bytes(account.user_id.data(), account.user_id.size())) {
    return std::unexpected(k_broken_record);
  }

  account.name = core::ContentBuffer(reader.rest());

  return account;
}

core::Result<core::ContentBuffer> encode_publication(const core::PublicationRecord &publication) {
  core::ContentBuffer data;

  core::append_integer(data, publication.publication_id, core::k_integer_size);
  core::append_bytes(data, publication.author_id.data(), publication.author_id.size());
  append_text(data, publication.author_name);
  append_text(data, publication.title);
  append_text(data, publication.file_name);
  core::append_integer(data, publication.size, core::k_integer_size);
  core::append_integer(data, static_cast<std::uint64_t>(publication.published_at), core::k_integer_size);
  core::append_bytes(data, publication.identity.data(), publication.identity.size());
  core::append_bytes(data, publication.author_key.data(), publication.author_key.size());
  core::append_integer(data, static_cast<std::uint8_t>(publication.signature_algorithm), 1);
  core::append_bytes(data, publication.signature.data(), publication.signature.size());

  return data;
}

core::Result<core::PublicationRecord> decode_publication(core::Content data) {
  core::Reader reader(data);

  core::PublicationRecord publication{};
  std::uint64_t publication_id = 0;
  std::uint64_t size = 0;
  std::uint64_t published_at = 0;
  std::uint64_t signature_algorithm = 0;

  if (!reader.read_integer(publication_id, core::k_integer_size) ||
      !reader.read_bytes(publication.author_id.data(), publication.author_id.size()) ||
      !read_text(reader, publication.author_name) || !read_text(reader, publication.title) ||
      !read_text(reader, publication.file_name) || !reader.read_integer(size, core::k_integer_size) ||
      !reader.read_integer(published_at, core::k_integer_size) ||
      !reader.read_bytes(publication.identity.data(), publication.identity.size()) ||
      !reader.read_bytes(publication.author_key.data(), publication.author_key.size()) ||
      !reader.read_integer(signature_algorithm, 1) ||
      !reader.read_bytes(publication.signature.data(), publication.signature.size()) || !reader.empty()) {
    return std::unexpected(k_broken_record);
  }

  publication.publication_id = publication_id;
  publication.size = static_cast<std::size_t>(size);
  publication.published_at = static_cast<core::Timestamp>(published_at);
  publication.signature_algorithm = static_cast<core::SignatureAlgorithm>(signature_algorithm);

  return publication;
}

core::Result<core::ContentBuffer> encode_purchase(const core::PurchaseRecord &purchase) {
  core::ContentBuffer data;

  core::append_integer(data, purchase.purchase_id, core::k_integer_size);
  core::append_bytes(data, purchase.user_id.data(), purchase.user_id.size());
  core::append_integer(data, purchase.publication_id, core::k_integer_size);
  core::append_integer(data, static_cast<std::uint64_t>(purchase.purchased_at), core::k_integer_size);

  return append_sealed(data, purchase.wrapped_blob_key);
}

core::Result<core::PurchaseRecord> decode_purchase(core::Content data) {
  core::Reader reader(data);

  core::PurchaseRecord purchase{};
  std::uint64_t purchase_id = 0;
  std::uint64_t publication_id = 0;
  std::uint64_t purchased_at = 0;

  if (!reader.read_integer(purchase_id, core::k_integer_size) ||
      !reader.read_bytes(purchase.user_id.data(), purchase.user_id.size()) ||
      !reader.read_integer(publication_id, core::k_integer_size) ||
      !reader.read_integer(purchased_at, core::k_integer_size)) {
    return std::unexpected(k_broken_record);
  }

  auto wrapped = core::decode_sealed_content(reader.rest());

  if (!wrapped.has_value()) {
    return std::unexpected(k_broken_record);
  }

  purchase.purchase_id = purchase_id;
  purchase.publication_id = publication_id;
  purchase.purchased_at = static_cast<core::Timestamp>(purchased_at);
  purchase.wrapped_blob_key = std::move(wrapped.value());

  return purchase;
}

core::Result<core::ContentBuffer> encode_receipt_record(const core::ReceiptRecord &record) {
  const auto receipt = core::encode_receipt(record.receipt);

  if (!receipt.has_value()) {
    return std::unexpected(k_broken_record);
  }

  core::ContentBuffer data;

  core::append_bytes(data, record.device_key.data(), record.device_key.size());
  data.append(receipt.value());

  return data;
}

core::Result<core::ReceiptRecord> decode_receipt_record(core::Content data) {
  core::Reader reader(data);

  core::ReceiptRecord record{};

  if (!reader.read_bytes(record.device_key.data(), record.device_key.size())) {
    return std::unexpected(k_broken_record);
  }

  auto receipt = core::decode_receipt(reader.rest());

  if (!receipt.has_value()) {
    return std::unexpected(k_broken_record);
  }

  record.receipt = std::move(receipt.value());

  return record;
}

} // namespace dgds::stubs
