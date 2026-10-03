#include <dgds/stubs/support/file_storage.h>

#include <dgds/core/envelope/keys.h>
#include <dgds/core/identity/content_identity.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace dgds::stubs {
namespace {

constexpr mode_t k_owner_only_file = 0600;
constexpr std::size_t k_secret_size = core::k_key_size;
constexpr const char *k_pending_suffix = ".pending";

enum class SecretCreation {
  created,
  already_present,
};

bool write_all(int descriptor, core::Content bytes) {
  std::size_t offset = 0;

  while (offset < bytes.size()) {
    const ssize_t count = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);

    if (count <= 0) {
      return false;
    }

    offset += static_cast<std::size_t>(count);
  }

  return true;
}

constexpr std::size_t k_unique_suffix_bytes = 8;

std::filesystem::path unique_pending(const std::filesystem::path &path) {
  std::array<std::uint8_t, k_unique_suffix_bytes> random{};

  if (RAND_bytes(random.data(), static_cast<int>(random.size())) != 1) {
    return {};
  }

  return path.string() + "." + core::to_hex(random.data(), random.size()) + k_pending_suffix;
}

void sync_directory(const std::filesystem::path &path) {
  const int descriptor = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW);

  if (descriptor < 0) {
    return;
  }

  ::fsync(descriptor);
  ::close(descriptor);
}

bool write_and_sync(int descriptor, core::Content bytes) {
  bool written = write_all(descriptor, bytes);

  if (written && ::fsync(descriptor) != 0) {
    written = false;
  }

  return written;
}

core::Result<SecretCreation> create_secret_file(const std::filesystem::path &path,
                                                const core::SecretBytes<k_secret_size> &secret) {
  const std::filesystem::path pending = unique_pending(path);

  if (pending.empty()) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  const int descriptor = ::open(pending.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, k_owner_only_file);

  if (descriptor < 0) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const core::Content bytes(reinterpret_cast<const char *>(secret.data()), secret.size());
  const bool written = write_and_sync(descriptor, bytes);
  const bool closed = ::close(descriptor) == 0;

  if (!written || !closed) {
    ::unlink(pending.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (::link(pending.c_str(), path.c_str()) != 0) {
    const bool taken = errno == EEXIST;
    ::unlink(pending.c_str());

    if (taken) {
      return SecretCreation::already_present;
    }

    return std::unexpected(core::CoreError::storage_failed);
  }

  ::unlink(pending.c_str());
  sync_directory(path.parent_path());

  return SecretCreation::created;
}

} // namespace

core::Result<void> ensure_directory(const std::filesystem::path &path) {
  if (path.empty()) {
    return {};
  }

  std::error_code status;
  const bool created = std::filesystem::create_directories(path, status);

  if (!status && created) {
    std::filesystem::permissions(path, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace,
                                 status);
  }

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return {};
}

core::Result<bool> claim_file(const std::filesystem::path &path) {
  const auto prepared = ensure_directory(path.parent_path());

  if (!prepared.has_value()) {
    return std::unexpected(prepared.error());
  }

  const int descriptor = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, k_owner_only_file);

  if (descriptor >= 0) {
    ::close(descriptor);
    return true;
  }

  if (errno == EEXIST) {
    return false;
  }

  return std::unexpected(core::CoreError::storage_failed);
}

core::Result<bool> file_exists(const std::filesystem::path &path) {
  std::error_code status;
  const bool present = std::filesystem::exists(path, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (!present) {
    return false;
  }

  const bool regular = std::filesystem::is_regular_file(path, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return regular;
}

core::Result<std::vector<std::filesystem::path>> list_files(const std::filesystem::path &directory) {
  std::vector<std::filesystem::path> files;

  std::error_code status;
  const bool present = std::filesystem::exists(directory, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (!present) {
    return files;
  }

  const std::filesystem::directory_iterator entries(directory, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  for (const auto &entry : entries) {
    const bool regular = entry.is_regular_file(status);

    if (status) {
      return std::unexpected(core::CoreError::storage_failed);
    }

    if (regular && entry.path().extension() != k_pending_suffix) {
      files.push_back(entry.path());
    }
  }

  std::sort(files.begin(), files.end());

  return files;
}

core::Result<core::ContentBuffer> load_file(const std::filesystem::path &path, core::CoreError missing) {
  const int descriptor = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW);

  if (descriptor < 0) {
    if (errno == ENOENT) {
      return std::unexpected(missing);
    }

    return std::unexpected(core::CoreError::storage_failed);
  }

  struct stat info{};
  const bool measured = ::fstat(descriptor, &info) == 0 && info.st_size >= 0;

  if (!measured) {
    ::close(descriptor);
    return std::unexpected(core::CoreError::storage_failed);
  }

  core::ContentBuffer data(static_cast<std::size_t>(info.st_size), '\0');
  std::size_t offset = 0;

  while (offset < data.size()) {
    const ssize_t count = ::read(descriptor, data.data() + offset, data.size() - offset);

    if (count <= 0) {
      ::close(descriptor);
      return std::unexpected(core::CoreError::storage_failed);
    }

    offset += static_cast<std::size_t>(count);
  }

  ::close(descriptor);

  return data;
}

core::Result<void> store_file(const std::filesystem::path &path, core::Content bytes) {
  const auto prepared = ensure_directory(path.parent_path());

  if (!prepared.has_value()) {
    return std::unexpected(prepared.error());
  }

  const std::filesystem::path pending = unique_pending(path);

  if (pending.empty()) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  const int descriptor = ::open(pending.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, k_owner_only_file);

  if (descriptor < 0) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const bool written = write_and_sync(descriptor, bytes);
  const bool closed = ::close(descriptor) == 0;

  if (!written || !closed) {
    ::unlink(pending.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

  std::error_code status;
  std::filesystem::rename(pending, path, status);

  if (status) {
    ::unlink(pending.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

  sync_directory(path.parent_path());

  return {};
}

core::Result<bool> store_file_if_absent(const std::filesystem::path &path, core::Content bytes) {
  const auto prepared = ensure_directory(path.parent_path());

  if (!prepared.has_value()) {
    return std::unexpected(prepared.error());
  }

  const std::filesystem::path pending = unique_pending(path);

  if (pending.empty()) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  const int descriptor = ::open(pending.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, k_owner_only_file);

  if (descriptor < 0) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const bool written = write_and_sync(descriptor, bytes);
  const bool closed = ::close(descriptor) == 0;

  if (!written || !closed) {
    ::unlink(pending.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (::link(pending.c_str(), path.c_str()) != 0) {
    const bool present = errno == EEXIST;
    ::unlink(pending.c_str());

    if (present) {
      return false;
    }

    return std::unexpected(core::CoreError::storage_failed);
  }

  ::unlink(pending.c_str());
  sync_directory(path.parent_path());

  return true;
}

core::Result<void> remove_file(const std::filesystem::path &path) {
  if (::unlink(path.c_str()) != 0 && errno != ENOENT) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return {};
}

core::Result<core::SecretBytes<k_secret_size>> load_secret(const std::filesystem::path &path) {
  const int descriptor = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW);

  if (descriptor < 0) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  std::array<std::uint8_t, k_secret_size> bytes{};
  std::size_t offset = 0;

  while (offset < bytes.size()) {
    const ssize_t count = ::read(descriptor, bytes.data() + offset, bytes.size() - offset);

    if (count <= 0) {
      ::close(descriptor);
      OPENSSL_cleanse(bytes.data(), bytes.size());
      return std::unexpected(count < 0 ? core::CoreError::storage_failed : core::CoreError::key_size_mismatch);
    }

    offset += static_cast<std::size_t>(count);
  }

  std::uint8_t excess = 0;
  const ssize_t beyond = ::read(descriptor, &excess, 1);
  ::close(descriptor);

  if (beyond != 0) {
    OPENSSL_cleanse(bytes.data(), bytes.size());
    return std::unexpected(core::CoreError::key_size_mismatch);
  }

  core::SecretBytes<k_secret_size> secret;
  std::copy(bytes.begin(), bytes.end(), secret.data());
  OPENSSL_cleanse(bytes.data(), bytes.size());

  return secret;
}

core::Result<core::SecretBytes<k_secret_size>> load_or_create_secret(const std::filesystem::path &path) {
  std::error_code status;
  const bool present = std::filesystem::exists(path, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (present) {
    return load_secret(path);
  }

  const auto prepared = ensure_directory(path.parent_path());

  if (!prepared.has_value()) {
    return std::unexpected(prepared.error());
  }

  auto generated = core::generate_key();

  if (!generated.has_value()) {
    return std::unexpected(generated.error());
  }

  const auto creation = create_secret_file(path, generated.value());

  if (!creation.has_value()) {
    return std::unexpected(creation.error());
  }

  if (creation.value() == SecretCreation::created) {
    return std::move(generated.value());
  }

  return load_secret(path);
}

} // namespace dgds::stubs
