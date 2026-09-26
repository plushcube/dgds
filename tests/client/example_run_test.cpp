#include "server_process.h"

#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/models/protocol.h>
#include <dgds/server/models/attribution.h>
#include <dgds/server/services/attribution_service.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using dgds::core::canonical_form;
using dgds::server::AccessKind;
using dgds::server::AttributionService;
using dgds::stubs::FileMetadataRegistry;
using dgds::test::ServerProcess;

constexpr std::string_view k_marker = "демонстрационная строка";
constexpr std::string_view k_sample_line = "демонстрационного текста для покупателя";

std::string sample_text() {
  std::string text;

  for (std::size_t line = 0; line < 20; ++line) {
    text += std::string(k_marker) + " " + std::to_string(line) + " для проверки вывода\n";
  }

  return text;
}

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
}

struct RunResult {
  int status;
  std::string output;
};

class ExampleRunTest : public ::testing::Test {
protected:
  ExampleRunTest() : m_root(temporary_path("root")), m_text(temporary_path("text")) {
    std::filesystem::create_directories(m_root);
    std::ofstream(m_text, std::ios::binary) << sample_text();
  }

  void SetUp() override {
    ASSERT_TRUE(m_server.start(m_root / "server"));
    std::filesystem::create_directories(device());
  }

  void TearDown() override {
    m_server.stop();
    std::filesystem::remove_all(m_root);
    std::filesystem::remove(m_text);
  }

  static std::filesystem::path temporary_path(std::string_view kind) {
    return std::filesystem::temp_directory_path() /
           ("dgds-example-" + std::string(kind) + "-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] std::filesystem::path device() const { return m_root / "device"; }

  // Клиент запускается с рабочим каталогом устройства: тогда проверка отсутствия
  // открытого текста покрывает и всё, что клиент мог бы оставить в текущем каталоге
  [[nodiscard]] RunResult run(std::string_view arguments) const {
    const std::string command = "cd '" + device().string() + "' && '" + std::string(DGDS_EXAMPLE_PATH) +
                                "' --certificate '" + m_server.certificate().string() + "' --port " +
                                std::to_string(m_server.port()) + " --device '" + device().string() + "' " +
                                std::string(arguments) + " 2>&1";
    FILE *pipe = popen(command.c_str(), "r");
    EXPECT_NE(pipe, nullptr);

    std::string output;
    char buffer[4096];
    std::size_t read = 0;

    while ((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
      output.append(buffer, read);
    }

    const int status = pclose(pipe);

    return RunResult{.status = WIFEXITED(status) ? WEXITSTATUS(status) : -1, .output = output};
  }

  std::filesystem::path m_root;
  std::filesystem::path m_text;
  ServerProcess m_server;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(ExampleRunTest, RefusesOptionValueThatLooksLikeOption) {
  const RunResult result = run("--text --version");

  EXPECT_EQ(result.status, 1);
  EXPECT_NE(result.output.find("--certificate"), std::string::npos);
}

TEST_F(ExampleRunTest, RunsTwiceOnSameStand) {
  const RunResult first = run("--text '" + m_text.string() + "'");

  ASSERT_EQ(first.status, 0) << first.output;
  EXPECT_NE(first.output.find("Публикация"), std::string::npos);

  const RunResult second = run("");

  ASSERT_EQ(second.status, 0) << second.output;
  EXPECT_NE(second.output.find("Публикация"), std::string::npos);
  EXPECT_NE(canonical_form(second.output).find(k_sample_line), std::string::npos);
}

TEST_F(ExampleRunTest, ReportsUnreadableTextFile) {
  const RunResult result = run("--text '" + (m_root / "нет-такого-файла").string() + "'");

  EXPECT_EQ(result.status, 1);
  EXPECT_NE(result.output.find("чтение файла"), std::string::npos);
  EXPECT_EQ(canonical_form(result.output).find(k_marker), std::string::npos);
}

TEST_F(ExampleRunTest, PrintsPurchasedContentAndLeavesNoPlaintext) {
  const RunResult result = run("--text '" + m_text.string() + "'");

  ASSERT_EQ(result.status, 0) << result.output;
  EXPECT_NE(result.output.find("Публикация"), std::string::npos);
  EXPECT_NE(result.output.find("Каталог     1 из 1 публикаций"), std::string::npos) << result.output;
  EXPECT_NE(canonical_form(result.output).find(k_marker), std::string::npos);

  EXPECT_TRUE(std::filesystem::exists(device() / "device.key")) << "устройство не создало ключ";

  std::size_t receipts = 0;

  for (const auto &entry : std::filesystem::directory_iterator(device() / "receipts")) {
    if (entry.is_regular_file() && entry.file_size() > 0) {
      ++receipts;
    }
  }

  EXPECT_EQ(receipts, 1U) << "проверка отсутствия открытого текста была бы пустой";

  for (const auto &entry : std::filesystem::recursive_directory_iterator(m_root)) {
    if (!entry.is_regular_file()) {
      continue;
    }

    EXPECT_EQ(canonical_form(read_file(entry.path())).find(k_marker), std::string::npos) << entry.path();
  }

  FileMetadataRegistry metadata{m_root / "server" / "metadata"};
  AttributionService attribution{metadata};

  const auto report = attribution.attribute(result.output);

  ASSERT_TRUE(report.has_value());
  EXPECT_EQ(report->kind, AccessKind::purchase);

  const auto publications = metadata.publications(0, dgds::core::k_default_page_size);
  ASSERT_TRUE(publications.has_value());
  ASSERT_EQ(publications->records.size(), 1U);
  EXPECT_EQ(report->publication_id, publications->records.front().publication_id);
  EXPECT_NE(report->user_id, publications->records.front().author_id);
}

TEST_F(ExampleRunTest, RefusesUnknownOption) {
  const RunResult result = run("--unknown");

  EXPECT_EQ(result.status, 1);
  EXPECT_NE(result.output.find("--certificate"), std::string::npos);
  EXPECT_EQ(canonical_form(result.output).find(k_marker), std::string::npos);
}

} // namespace
