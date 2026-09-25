#include <httplib.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <poll.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using Json = nlohmann::json;

constexpr int k_readiness_timeout_ms = 10000;
constexpr const char *k_listen_marker = "Слушаю https://127.0.0.1:";

class LiveServerTest : public ::testing::Test {
protected:
  LiveServerTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void SetUp() override { start_server(); }

  void TearDown() override {
    stop_server();
    std::filesystem::remove_all(m_root);
  }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-live-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  void start_server() {
    int pipes[2] = {-1, -1};
    ASSERT_EQ(pipe(pipes), 0);

    const pid_t child = fork();
    ASSERT_GE(child, 0);

    if (child == 0) {
      dup2(pipes[1], STDOUT_FILENO);
      close(pipes[0]);
      close(pipes[1]);
      execl(DGDS_SERVER_PATH, DGDS_SERVER_PATH, "--root", m_root.c_str(), "--port", "0", nullptr);
      _exit(127);
    }

    close(pipes[1]);
    m_child = child;

    FILE *output = fdopen(pipes[0], "r");
    ASSERT_NE(output, nullptr);

    pollfd readable{.fd = pipes[0], .events = POLLIN, .revents = 0};
    ASSERT_GT(poll(&readable, 1, k_readiness_timeout_ms), 0) << "сервер не объявил адрес за отведённое время";

    char buffer[512] = {};
    int port = 0;

    while (fgets(buffer, sizeof(buffer), output) != nullptr) {
      const std::string line{buffer};
      const std::size_t marker = line.find(k_listen_marker);

      if (marker == std::string::npos) {
        continue;
      }

      port = std::atoi(line.substr(marker + std::string(k_listen_marker).size()).c_str());
      break;
    }

    fclose(output);

    ASSERT_GT(port, 0);
    m_port = port;
  }

  void stop_server() {
    if (m_child <= 0) {
      return;
    }

    kill(m_child, SIGTERM);

    int status = 0;
    waitpid(m_child, &status, 0);
    m_child = -1;
  }

  [[nodiscard]] httplib::Result post(const std::string &path, const std::string &body) {
    httplib::SSLClient client{"127.0.0.1", m_port};
    client.set_ca_cert_path((m_root / "tls" / "server.crt").string());
    client.enable_server_certificate_verification(true);
    client.set_connection_timeout(5);

    return client.Post(path, body, "application/json");
  }

  std::filesystem::path m_root;
  pid_t m_child = -1;
  int m_port = 0;

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(LiveServerTest, AnswersOverTlsWithVerifiedCertificate) {
  const auto response = post("/catalog", R"({"version":1})");

  ASSERT_TRUE(response) << "соединение не установлено: " << httplib::to_string(response.error());
  EXPECT_EQ(response->status, 200);

  const Json body = Json::parse(response->body);
  EXPECT_EQ(body.at("version").get<std::uint8_t>(), 1);
  EXPECT_TRUE(body.at("data").is_array());
  EXPECT_TRUE(body.at("data").empty());
}

TEST_F(LiveServerTest, RefusesMalformedRequest) {
  const auto response = post("/purchases", R"({"version":1})");

  ASSERT_TRUE(response);
  EXPECT_EQ(response->status, 400);
  EXPECT_EQ(Json::parse(response->body).at("error").at("code").get<std::string>(), "request_malformed");
}

TEST_F(LiveServerTest, KeepsCertificateBetweenRuns) {
  const std::filesystem::path certificate = m_root / "tls" / "server.crt";
  ASSERT_TRUE(std::filesystem::exists(certificate));

  const auto issued = std::filesystem::last_write_time(certificate);

  stop_server();
  start_server();

  EXPECT_EQ(std::filesystem::last_write_time(certificate), issued);

  const auto response = post("/catalog", R"({"version":1})");

  ASSERT_TRUE(response) << "после перезапуска соединение не установлено";
  EXPECT_EQ(response->status, 200);
}

} // namespace
