#include <dgds/core/identity/canonical_form.h>
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

constexpr std::string_view k_marker = "демонстрационная строка";

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

  void TearDown() override {
    std::filesystem::remove_all(m_root);
    std::filesystem::remove(m_text);
  }

  static std::filesystem::path temporary_path(std::string_view kind) {
    return std::filesystem::temp_directory_path() /
           ("dgds-example-" + std::string(kind) + "-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] RunResult run(std::string_view arguments) const {
    const std::string command =
        "'" + std::string(DGDS_EXAMPLE_PATH) + "' --root '" + m_root.string() + "' " + std::string(arguments) + " 2>&1";
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

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(ExampleRunTest, PrintsPurchasedContentAndLeavesNoPlaintext) {
  const RunResult result = run("--text '" + m_text.string() + "'");

  ASSERT_EQ(result.status, 0) << result.output;
  EXPECT_NE(canonical_form(result.output).find(k_marker), std::string::npos);

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

  const auto publications = metadata.publications();
  ASSERT_TRUE(publications.has_value());
  ASSERT_EQ(publications->size(), 1U);
  EXPECT_EQ(report->publication_id, publications->front().publication_id);
  EXPECT_NE(report->user_id, publications->front().author_id);
}

TEST_F(ExampleRunTest, RefusesUnknownOption) {
  const RunResult result = run("--unknown");

  EXPECT_EQ(result.status, 1);
  EXPECT_NE(result.output.find("--root"), std::string::npos);
  EXPECT_EQ(canonical_form(result.output).find(k_marker), std::string::npos);
}

} // namespace
