#include <dgds/stubs/device_key/file_device_key.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/receipt.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace {

using dgds::core::CoreError;
using dgds::core::generate_device_key;
using dgds::core::k_receipt_version;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::SymmetricKey;
using dgds::core::UserId;
using dgds::core::wrap_receipt_key;
using dgds::stubs::FileDeviceKey;

SymmetricKey make_key(std::uint8_t seed) {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key.data()[index] = static_cast<std::uint8_t>(seed + index);
  }

  return key;
}

UserId make_user_id(std::uint8_t seed) {
  UserId user_id{};

  for (std::size_t index = 0; index < user_id.size(); ++index) {
    user_id[index] = static_cast<std::uint8_t>(seed * 3 + index);
  }

  return user_id;
}

ReceiptHeader make_header(std::uint8_t seed) {
  return ReceiptHeader{.version = k_receipt_version,
                       .purchase_id = seed,
                       .user_id = make_user_id(seed),
                       .purchased_at = 1700000000 + seed,
                       .issued_at = 1700000100 + seed};
}

class DeviceKeyTest : public ::testing::Test {
protected:
  void SetUp() override {
    m_root = std::filesystem::temp_directory_path() /
             ("dgds-device-key-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
    std::filesystem::create_directories(m_root);
  }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  [[nodiscard]] std::filesystem::path key_path() const { return m_root / "device.key"; }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(DeviceKeyTest, KeepsKeyAcrossInstances) {
  FileDeviceKey first(key_path());
  FileDeviceKey second(key_path());

  const auto initial = first.public_key();
  const auto repeated = second.public_key();

  ASSERT_TRUE(initial.has_value());
  ASSERT_TRUE(repeated.has_value());
  EXPECT_EQ(initial.value(), repeated.value());
}

TEST_F(DeviceKeyTest, OpensReceiptOfOwnDevice) {
  FileDeviceKey device(key_path());
  const SymmetricKey purchase_key = make_key(1);
  const ReceiptHeader header = make_header(1);

  const auto public_key = device.public_key();
  ASSERT_TRUE(public_key.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, public_key.value(), header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  const auto opened = device.open_receipt_key(receipt);

  ASSERT_TRUE(opened.has_value());
  EXPECT_TRUE(opened->equals(purchase_key));
}

TEST_F(DeviceKeyTest, RejectsReceiptOfAnotherDevice) {
  FileDeviceKey device(key_path());
  const SymmetricKey purchase_key = make_key(2);
  const ReceiptHeader header = make_header(2);

  const auto other = generate_device_key();
  ASSERT_TRUE(other.has_value());

  const auto wrapped = wrap_receipt_key(purchase_key, other->public_key, header);
  ASSERT_TRUE(wrapped.has_value());

  const Receipt receipt{.header = header, .wrapped_key = wrapped.value()};

  const auto opened = device.open_receipt_key(receipt);

  ASSERT_FALSE(opened.has_value());
  EXPECT_EQ(opened.error(), CoreError::authentication_failed);
}

TEST_F(DeviceKeyTest, RestrictsKeyFileToOwner) {
  FileDeviceKey device(key_path());

  const auto public_key = device.public_key();
  ASSERT_TRUE(public_key.has_value());

  constexpr std::filesystem::perms k_shared = std::filesystem::perms::group_all | std::filesystem::perms::others_all;
  const auto permissions = std::filesystem::status(key_path()).permissions();

  EXPECT_EQ(permissions & k_shared, std::filesystem::perms::none);
}

TEST_F(DeviceKeyTest, RejectsTruncatedKeyFile) {
  std::ofstream truncated(key_path(), std::ios::binary);
  truncated << "short";
  truncated.close();

  FileDeviceKey device(key_path());

  const auto public_key = device.public_key();

  ASSERT_FALSE(public_key.has_value());
  EXPECT_EQ(public_key.error(), CoreError::key_size_mismatch);
}

TEST_F(DeviceKeyTest, ReportsStorageFailure) {
  std::ofstream blocker(key_path(), std::ios::binary);
  blocker << "не каталог";
  blocker.close();

  FileDeviceKey device(key_path() / "device.key");

  const auto public_key = device.public_key();

  ASSERT_FALSE(public_key.has_value());
  EXPECT_EQ(public_key.error(), CoreError::storage_failed);
}

TEST_F(DeviceKeyTest, CreatesDirectoryForKey) {
  FileDeviceKey device(m_root / "nested" / "device.key");

  const auto public_key = device.public_key();
  ASSERT_TRUE(public_key.has_value());

  constexpr std::filesystem::perms k_shared = std::filesystem::perms::group_all | std::filesystem::perms::others_all;
  const auto permissions = std::filesystem::status(m_root / "nested").permissions();

  EXPECT_EQ(permissions & k_shared, std::filesystem::perms::none);
}

} // namespace
