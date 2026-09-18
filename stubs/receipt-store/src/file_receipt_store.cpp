#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <cerrno>
#include <cstddef>
#include <expected>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace dgds::stubs {
namespace {

constexpr mode_t k_owner_only_file = 0600;
constexpr const char *k_pending_suffix = ".pending";

bool write_all(int descriptor, client::Content blob) {
  std::size_t offset = 0;

  while (offset < blob.size()) {
    const ssize_t count = ::write(descriptor, blob.data() + offset, blob.size() - offset);

    if (count <= 0) {
      return false;
    }

    offset += static_cast<std::size_t>(count);
  }

  return true;
}

core::Result<client::ContentBuffer> read_all(int descriptor, std::size_t size) {
  client::ContentBuffer blob(size, '\0');
  std::size_t offset = 0;

  while (offset < blob.size()) {
    const ssize_t count = ::read(descriptor, blob.data() + offset, blob.size() - offset);

    if (count <= 0) {
      return std::unexpected(core::CoreError::storage_failed);
    }

    offset += static_cast<std::size_t>(count);
  }

  return blob;
}

} // namespace

client::Result<void> FileReceiptStore::save(const client::PurchaseId &purchase_id, client::Content blob) {
  std::error_code status;
  std::filesystem::create_directories(m_root, status);

  if (!status) {
    std::filesystem::permissions(m_root, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace,
                                 status);
  }

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const std::filesystem::path pending = path_of(purchase_id).string() + k_pending_suffix;
  const int descriptor = ::open(pending.c_str(), O_CREAT | O_TRUNC | O_WRONLY, k_owner_only_file);

  if (descriptor < 0) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const bool written = write_all(descriptor, blob);
  const bool closed = ::close(descriptor) == 0;

  if (!written || !closed) {
    ::unlink(pending.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

  std::filesystem::rename(pending, path_of(purchase_id), status);

  if (status) {
    ::unlink(pending.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

  return {};
}

client::Result<client::ContentBuffer> FileReceiptStore::load(const client::PurchaseId &purchase_id) {
  const int descriptor = ::open(path_of(purchase_id).c_str(), O_RDONLY);

  if (descriptor < 0) {
    if (errno == ENOENT) {
      return std::unexpected(core::CoreError::receipt_not_found);
    }

    return std::unexpected(core::CoreError::storage_failed);
  }

  struct stat info{};
  const bool measured = ::fstat(descriptor, &info) == 0 && info.st_size >= 0;

  if (!measured) {
    ::close(descriptor);
    return std::unexpected(core::CoreError::storage_failed);
  }

  auto blob = read_all(descriptor, static_cast<std::size_t>(info.st_size));
  ::close(descriptor);

  return blob;
}

} // namespace dgds::stubs
