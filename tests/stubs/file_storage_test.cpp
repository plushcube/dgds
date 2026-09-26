#include <dgds/stubs/support/file_storage.h>

#include <dgds/core/models/crypto.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <latch>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

using dgds::stubs::load_or_create_secret;
using dgds::stubs::store_file;
using dgds::stubs::store_file_if_absent;

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);

  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

class FileStorageTest : public ::testing::Test {
protected:
  FileStorageTest() : m_root(root_directory()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path root_directory() {
    return std::filesystem::temp_directory_path() /
           ("dgds-file-storage-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] std::vector<std::filesystem::path> entries() const {
    std::vector<std::filesystem::path> found;

    for (const auto &entry : std::filesystem::directory_iterator(m_root)) {
      found.push_back(entry.path());
    }

    return found;
  }

  std::filesystem::path m_root;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(FileStorageTest, KeepsOneOfConcurrentWritesWhole) {
  const std::filesystem::path target = m_root / "record";
  constexpr std::size_t k_writers = 8;

  std::vector<std::string> payloads;

  for (std::size_t index = 0; index < k_writers; ++index) {
    payloads.push_back(std::string(4096, static_cast<char>('a' + static_cast<int>(index))));
  }

  std::latch ready(k_writers);
  std::vector<std::thread> writers;

  for (std::size_t index = 0; index < k_writers; ++index) {
    writers.emplace_back([&, index] {
      ready.arrive_and_wait();
      EXPECT_TRUE(store_file(target, payloads[index]).has_value());
    });
  }

  for (auto &writer : writers) {
    writer.join();
  }

  const std::string stored = read_file(target);

  EXPECT_NE(std::find(payloads.begin(), payloads.end(), stored), payloads.end())
      << "на диск попала смесь нескольких записей, длина " << stored.size();

  for (const auto &entry : entries()) {
    EXPECT_NE(entry.extension(), ".pending") << "остался временный файл " << entry;
  }
}

TEST_F(FileStorageTest, GivesExclusiveStoreToSingleWriter) {
  const std::filesystem::path target = m_root / "exclusive";
  constexpr std::size_t k_writers = 8;

  std::latch ready(k_writers);
  std::atomic<unsigned> winners{0};
  std::vector<std::thread> writers;

  for (std::size_t index = 0; index < k_writers; ++index) {
    writers.emplace_back([&, index] {
      ready.arrive_and_wait();
      const auto stored = store_file_if_absent(target, std::string(64, static_cast<char>('0' + index)));

      if (stored.has_value() && stored.value()) {
        winners.fetch_add(1);
      }
    });
  }

  for (auto &writer : writers) {
    writer.join();
  }

  EXPECT_EQ(winners.load(), 1U);
  EXPECT_TRUE(std::filesystem::exists(target));
  EXPECT_EQ(read_file(target).size(), 64U);
}

TEST_F(FileStorageTest, ReplacesSymlinkInsteadOfWritingThroughIt) {
  const std::filesystem::path outside = m_root / "outside";
  const std::filesystem::path target = m_root / "record";

  std::ofstream(outside) << "чужое содержимое";
  std::filesystem::create_symlink(outside, target);

  EXPECT_TRUE(store_file(target, std::string("наши данные")).has_value());

  EXPECT_EQ(read_file(outside), "чужое содержимое");
  EXPECT_FALSE(std::filesystem::is_symlink(target));
  EXPECT_EQ(read_file(target), "наши данные");
}

TEST_F(FileStorageTest, SharesOneSecretBetweenConcurrentCreators) {
  const std::filesystem::path path = m_root / "master.key";
  constexpr std::size_t k_writers = 8;

  std::latch ready(k_writers);
  std::vector<std::string> keys(k_writers);
  std::vector<std::thread> writers;

  for (std::size_t index = 0; index < k_writers; ++index) {
    writers.emplace_back([&, index] {
      ready.arrive_and_wait();
      const auto secret = load_or_create_secret(path);

      if (secret.has_value()) {
        keys[index] = std::string(reinterpret_cast<const char *>(secret->data()), secret->size());
      }
    });
  }

  for (auto &writer : writers) {
    writer.join();
  }

  for (const auto &key : keys) {
    EXPECT_EQ(key.size(), dgds::core::k_key_size);
    EXPECT_EQ(key, keys.front()) << "создатели мастер-ключа получили разные ключи";
  }
}

} // namespace
