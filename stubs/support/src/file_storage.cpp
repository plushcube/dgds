#include <dgds/stubs/support/file_storage.h>

#include <dgds/core/envelope/keys.h>

#include <openssl/crypto.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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

core::Result<SecretCreation> create_secret_file(const std::filesystem::path &path,
                                                const core::SecretBytes<k_secret_size> &secret) {
  const int descriptor = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, k_owner_only_file);

  if (descriptor < 0) {
    if (errno == EEXIST) {
      return SecretCreation::already_present;
    }

    return std::unexpected(core::CoreError::storage_failed);
  }

  const core::Content bytes(reinterpret_cast<const char *>(secret.data()), secret.size());
  const bool written = write_all(descriptor, bytes);

  if (!written || ::close(descriptor) != 0) {
    ::unlink(path.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

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

core::Result<bool> directory_is_empty(const std::filesystem::path &path) {
  std::error_code status;
  const bool present = std::filesystem::exists(path, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  if (!present) {
    return true;
  }

  const std::filesystem::directory_iterator entries(path, status);

  if (status) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  return entries == std::filesystem::directory_iterator();
}

core::Result<core::ContentBuffer> load_file(const std::filesystem::path &path, core::CoreError missing) {
  const int descriptor = ::open(path.c_str(), O_RDONLY);

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

  const std::filesystem::path pending = path.string() + k_pending_suffix;
  const int descriptor = ::open(pending.c_str(), O_CREAT | O_TRUNC | O_WRONLY, k_owner_only_file);

  if (descriptor < 0) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  const bool written = write_all(descriptor, bytes);
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

  return {};
}

core::Result<core::SecretBytes<k_secret_size>> load_secret(const std::filesystem::path &path) {
  const int descriptor = ::open(path.c_str(), O_RDONLY);

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
