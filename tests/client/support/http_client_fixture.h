#pragma once

#include "server_process.h"

#include <dgds/client/client.h>
#include <dgds/client/http/http_transport.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/models/protocol.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/stubs/device_key/file_device_key.h>
#include <dgds/stubs/receipt_store/file_receipt_store.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unistd.h>

namespace dgds::test {

inline constexpr std::string_view k_publication_title = "название";
inline constexpr std::string_view k_publication_line = "строка публикуемого текста для проверки\n";

inline std::string sample_content() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += std::string(k_publication_line);
  }

  return text;
}

class HttpClientFixture : public ::testing::Test {
protected:
  HttpClientFixture() : m_root(temporary_root()) {}

  void SetUp() override {
    std::filesystem::create_directories(device());
    ASSERT_TRUE(m_server.start(m_root / "server"));

    const auto trust = client::load_server_trust(m_server.certificate());
    ASSERT_TRUE(trust.has_value());

    m_transport = std::make_unique<client::HttpTransport>(
        client::Endpoint{.host = "127.0.0.1", .port = m_server.port()}, trust.value());
    m_client = std::make_unique<client::ApiClient>(*m_transport, m_device_key, m_receipts);
  }

  void TearDown() override {
    m_client.reset();
    m_transport.reset();
    m_server.stop();
    std::filesystem::remove_all(m_root);
  }

  [[nodiscard]] static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-http-client-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] core::Credentials sign_in(std::string_view name) {
    const auto account = m_client->register_user(name);

    if (!account.has_value()) {
      EXPECT_EQ(account.error(), core::CoreError::user_name_taken);
    }

    const auto credentials = m_client->log_in(name);
    EXPECT_TRUE(credentials.has_value());

    return credentials.value_or(core::Credentials{});
  }

  [[nodiscard]] core::PublicationId publish(std::string_view name, std::string_view text) {
    const auto credentials = sign_in(name);
    const auto keys = core::generate_author_key();
    EXPECT_TRUE(keys.has_value());

    const auto identity = core::content_identity(text);
    EXPECT_TRUE(identity.has_value());

    const auto signature = core::sign_author(identity.value(), name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    const core::PublicationDraft draft{
        .title = std::string(k_publication_title), .file_name = "файл.txt", .content = std::string(text)};
    const auto publication = m_client->publish(credentials, draft, keys->public_key, signature.value());
    EXPECT_TRUE(publication.has_value());

    return publication.value_or(0);
  }

  [[nodiscard]] std::filesystem::path device() const { return m_root / "device"; }

  [[nodiscard]] core::DevicePublicKey device_public_key() {
    const auto key = m_device_key.public_key();
    EXPECT_TRUE(key.has_value());

    return key.value_or(core::DevicePublicKey{});
  }

  std::filesystem::path m_root;
  ServerProcess m_server;
  stubs::FileDeviceKey m_device_key{device() / "device.key"};
  stubs::FileReceiptStore m_receipts{device() / "receipts"};
  std::unique_ptr<client::HttpTransport> m_transport;
  std::unique_ptr<client::ApiClient> m_client;

private:
  static inline std::atomic<unsigned> counter{0};
};

} // namespace dgds::test
