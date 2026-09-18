#include <dgds/stubs/device_key/file_device_key.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/receipt.h>

#include <openssl/crypto.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace dgds::stubs {
namespace {

constexpr mode_t k_owner_only = 0600;

enum class KeyCreation {
  created,
  already_present,
};

core::Result<core::DevicePrivateKey> read_private_key(const std::filesystem::path &path) {
  const int descriptor = ::open(path.c_str(), O_RDONLY);

  if (descriptor < 0) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  std::array<std::uint8_t, core::k_device_key_size> bytes{};
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

  std::uint8_t beyond = 0;
  const ssize_t excess = ::read(descriptor, &beyond, 1);
  ::close(descriptor);

  if (excess != 0) {
    OPENSSL_cleanse(bytes.data(), bytes.size());
    return std::unexpected(core::CoreError::key_size_mismatch);
  }

  core::DevicePrivateKey key;
  std::copy(bytes.begin(), bytes.end(), key.data());
  OPENSSL_cleanse(bytes.data(), bytes.size());

  return key;
}

core::Result<KeyCreation> create_private_key(const std::filesystem::path &path, const core::DevicePrivateKey &key) {
  const int descriptor = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, k_owner_only);

  if (descriptor < 0) {
    if (errno == EEXIST) {
      return KeyCreation::already_present;
    }

    return std::unexpected(core::CoreError::storage_failed);
  }

  std::size_t offset = 0;
  bool written = true;

  while (offset < key.size() && written) {
    const ssize_t count = ::write(descriptor, key.data() + offset, key.size() - offset);

    if (count <= 0) {
      written = false;
      break;
    }

    offset += static_cast<std::size_t>(count);
  }

  if (!written || ::close(descriptor) != 0) {
    ::unlink(path.c_str());
    return std::unexpected(core::CoreError::storage_failed);
  }

  return KeyCreation::created;
}

} // namespace

core::Result<core::DevicePrivateKey> FileDeviceKey::private_key() const {
  auto generated = core::generate_device_key();

  if (!generated.has_value()) {
    return std::unexpected(generated.error());
  }

  const auto creation = create_private_key(m_path, generated->private_key);

  if (!creation.has_value()) {
    return std::unexpected(creation.error());
  }

  if (creation.value() == KeyCreation::created) {
    return std::move(generated->private_key);
  }

  return read_private_key(m_path);
}

client::Result<client::DevicePublicKey> FileDeviceKey::public_key() {
  const auto key = private_key();

  if (!key.has_value()) {
    return std::unexpected(key.error());
  }

  return core::derive_device_public_key(key.value());
}

client::Result<client::SymmetricKey> FileDeviceKey::open_receipt_key(const client::Receipt &receipt) {
  const auto key = private_key();

  if (!key.has_value()) {
    return std::unexpected(key.error());
  }

  return core::open_receipt_key(receipt, key.value());
}

} // namespace dgds::stubs
