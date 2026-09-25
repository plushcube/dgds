#include "server_process.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace {

using Json = nlohmann::json;

class LiveServerTest : public ::testing::Test {
protected:
  void SetUp() override { ASSERT_TRUE(m_server.start(dgds::test::ServerProcess::temporary_root("dgds-live"))); }

  void TearDown() override {
    m_server.stop();
    std::filesystem::remove_all(m_server.root());
  }

  [[nodiscard]] httplib::Result post(const std::string &path, const std::string &body) {
    httplib::SSLClient client{"127.0.0.1", m_server.port()};
    client.set_ca_cert_path(m_server.certificate().string());
    client.enable_server_certificate_verification(true);
    client.set_connection_timeout(5);

    return client.Post(path, body, "application/json");
  }

  dgds::test::ServerProcess m_server;
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
  const std::filesystem::path certificate = m_server.certificate();
  ASSERT_TRUE(std::filesystem::exists(certificate));

  const auto issued = std::filesystem::last_write_time(certificate);
  const std::filesystem::path root = m_server.root();

  m_server.stop();
  ASSERT_TRUE(m_server.start(root));

  EXPECT_EQ(std::filesystem::last_write_time(certificate), issued);

  const auto response = post("/catalog", R"({"version":1})");

  ASSERT_TRUE(response) << "после перезапуска соединение не установлено";
  EXPECT_EQ(response->status, 200);
}

} // namespace
