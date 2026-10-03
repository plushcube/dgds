#include <dgds/stubs/blob_store/file_blob_store.h>

#include <dgds/core/crypto/sealed_content_codec.h>
#include <dgds/stubs/support/file_storage.h>

#include <expected>

namespace dgds::stubs {

core::Result<void> FileBlobStore::store(const core::ContentIdentity &identity, const core::SealedContent &blob) {
  const auto encoded = core::encode_sealed_content(blob);

  if (!encoded.has_value()) {
    return std::unexpected(core::CoreError::blob_malformed);
  }

  return store_file(path_of(identity), encoded.value());
}

core::Result<core::SealedContent> FileBlobStore::load(const core::ContentIdentity &identity) {
  const auto stored = load_file(path_of(identity), core::CoreError::blob_not_found);

  if (!stored.has_value()) {
    return std::unexpected(stored.error());
  }

  const auto blob = core::decode_sealed_content(stored.value());

  if (!blob.has_value()) {
    return std::unexpected(core::CoreError::blob_malformed);
  }

  return blob;
}

} // namespace dgds::stubs
