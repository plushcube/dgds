#include <dgds/server/api/errors.h>
#include <dgds/server/api/surface.h>
#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/core/envelope/device_wrap.h>
#include <dgds/core/envelope/package.h>
#include <dgds/core/envelope/receipt.h>
#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/user_id.h>
#include <dgds/core/models/protocol.h>
#include <dgds/core/signature/author_signature.h>
#include <dgds/stubs/blob_store/file_blob_store.h>
#include <dgds/stubs/identity_registry/file_identity_registry.h>
#include <dgds/stubs/key_store/file_key_store.h>
#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

using Json = nlohmann::json;

using dgds::core::canonical_form;
using dgds::core::content_identity;
using dgds::core::CoreError;
using dgds::core::DeviceEnvelope;
using dgds::core::generate_author_key;
using dgds::core::generate_device_key;
using dgds::core::k_package_version;
using dgds::core::open_package;
using dgds::core::open_receipt_key;
using dgds::core::Package;
using dgds::core::Receipt;
using dgds::core::ReceiptHeader;
using dgds::core::SealedContent;
using dgds::core::sign_author;
using dgds::core::to_hex;
using dgds::server::CatalogService;
using dgds::server::DeliveryService;
using dgds::server::PublicationService;
using dgds::server::PurchaseService;
using dgds::server::SessionStore;
using dgds::server::UserService;
using dgds::server::api::code_of;
using dgds::server::api::ProtocolError;
using dgds::server::api::Surface;
using dgds::stubs::FileBlobStore;
using dgds::stubs::FileIdentityRegistry;
using dgds::stubs::FileKeyStore;
using dgds::stubs::FileMetadataRegistry;

constexpr std::string_view k_text = "строка публикуемого текста для проверки поверхности\n";
constexpr std::string_view k_title = "название";

std::string long_text() {
  std::string text;

  for (std::size_t line = 0; line < 10; ++line) {
    text += std::string(k_text);
  }

  return text;
}

template <std::size_t Size> std::array<std::uint8_t, Size> bytes_of(const std::string &hex) {
  const auto decoded = dgds::core::from_hex(hex);
  EXPECT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded.value_or(std::string{}).size(), Size);

  std::array<std::uint8_t, Size> bytes{};
  std::copy_n(decoded.value_or(std::string{}).begin(), Size, bytes.begin());

  return bytes;
}

SealedContent sealed_content_of(const Json &value) {
  return SealedContent{.algorithm = static_cast<dgds::core::AeadAlgorithm>(value.at("algorithm").get<std::uint8_t>()),
                       .nonce = bytes_of<dgds::core::k_nonce_size>(value.at("nonce").get<std::string>()),
                       .ciphertext = {},
                       .tag = bytes_of<dgds::core::k_tag_size>(value.at("tag").get<std::string>())};
}

void attach_ciphertext(SealedContent &target, const Json &value) {
  const auto decoded = dgds::core::from_hex(value.at("ciphertext").get<std::string>());
  ASSERT_TRUE(decoded.has_value());
  target.ciphertext.assign(decoded->begin(), decoded->end());
}

DeviceEnvelope envelope_of(const Json &value) {
  DeviceEnvelope envelope{.ephemeral_key =
                              bytes_of<dgds::core::k_device_key_size>(value.at("ephemeral_key").get<std::string>()),
                          .wrapped = sealed_content_of(value.at("wrapped"))};

  attach_ciphertext(envelope.wrapped, value.at("wrapped"));

  return envelope;
}

Receipt receipt_of(const Json &value) {
  const Json &header = value.at("header");

  return Receipt{.header = ReceiptHeader{.version = header.at("version").get<std::uint8_t>(),
                                         .purchase_id = std::stoull(header.at("purchase_id").get<std::string>()),
                                         .user_id = dgds::core::from_uuid(header.at("user_id").get<std::string>())
                                                        .value_or(dgds::core::UserId{}),
                                         .purchased_at = header.at("purchased_at").get<dgds::core::Timestamp>(),
                                         .issued_at = header.at("issued_at").get<dgds::core::Timestamp>()},
                 .wrapped_key = envelope_of(value.at("wrapped_key"))};
}

Package package_of(const Json &value) {
  Package package{.version = value.at("version").get<std::uint8_t>(),
                  .content = sealed_content_of(value.at("content")),
                  .wrapped_blob_key = sealed_content_of(value.at("wrapped_blob_key")),
                  .identity = bytes_of<dgds::core::k_identity_size>(value.at("identity").get<std::string>()),
                  .author_key =
                      bytes_of<dgds::core::k_author_public_key_size>(value.at("author_key").get<std::string>()),
                  .author_name = value.at("author_name").get<std::string>(),
                  .signature_algorithm =
                      static_cast<dgds::core::SignatureAlgorithm>(value.at("signature_algorithm").get<std::uint8_t>()),
                  .signature = bytes_of<dgds::core::k_signature_size>(value.at("signature").get<std::string>())};

  attach_ciphertext(package.content, value.at("content"));
  attach_ciphertext(package.wrapped_blob_key, value.at("wrapped_blob_key"));

  return package;
}

class SurfaceTest : public ::testing::Test {
protected:
  SurfaceTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-surface-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  [[nodiscard]] Json response_of(const std::string &wire) {
    const Json parsed = Json::parse(wire);
    EXPECT_TRUE(parsed.contains("data") || parsed.contains("error"));
    EXPECT_EQ(parsed.at("version").get<std::uint8_t>(), dgds::core::k_protocol_version);

    return parsed;
  }

  [[nodiscard]] std::string request_of(const Json &payload) const {
    Json body = payload;
    body["version"] = dgds::core::k_protocol_version;

    return body.dump();
  }

  [[nodiscard]] std::string account_of(std::string_view name) {
    return response_of(m_surface.register_user(request_of(Json{{"name", name}}))).at("data").dump();
  }

  [[nodiscard]] std::string credentials_of(const std::string &account) {
    const Json parsed = Json::parse(account);

    return response_of(m_surface.log_in(request_of(Json{{"name", parsed.at("name")}}))).at("data").dump();
  }

  std::filesystem::path m_root;
  FileIdentityRegistry m_identities{m_root / "identities"};
  FileKeyStore m_keys{m_root / "master.key", m_root / "keys"};
  FileBlobStore m_blobs{m_root / "blobs"};
  FileMetadataRegistry m_metadata{m_root / "metadata"};
  SessionStore m_sessions;
  UserService m_users{m_metadata, m_sessions};
  CatalogService m_catalog{m_metadata};
  PublicationService m_publications{m_identities, m_keys, m_blobs, m_metadata};
  PurchaseService m_purchases{m_keys, m_metadata};
  DeliveryService m_delivery{m_blobs, m_keys, m_metadata};

  dgds::core::Timestamp m_now = 1700000000;
  Surface m_surface{
      m_users, m_sessions, m_catalog, m_publications, m_purchases, m_delivery, [this]() { return m_now; }};

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(SurfaceTest, RunsScenarioAcrossEveryOperation) {
  const std::string author_account = account_of("автор");
  const std::string buyer_account = account_of("покупатель");

  const Json author = Json::parse(author_account);
  const Json buyer = Json::parse(buyer_account);
  EXPECT_NE(author.at("user_id").get<std::string>(), buyer.at("user_id").get<std::string>());

  const std::string author_credentials = credentials_of(author_account);
  const std::string buyer_credentials = credentials_of(buyer_account);

  const std::string text = long_text();
  const auto keys = generate_author_key();
  const auto identity = content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const Json publication_request{
      {"credentials", Json::parse(author_credentials)},
      {"draft", Json{{"title", std::string(k_title)}, {"file_name", "файл.txt"}, {"content", text}}},
      {"author_key", to_hex(keys->public_key.data(), keys->public_key.size())},
      {"signature", to_hex(signature->data(), signature->size())}};

  const auto published = response_of(m_surface.publish(request_of(publication_request)));
  ASSERT_TRUE(published.contains("data")) << published.dump();

  const std::string publication_id = published.at("data").get<std::string>();

  const auto catalog = response_of(m_surface.catalog(request_of(Json::object())));

  ASSERT_TRUE(catalog.contains("data")) << catalog.dump();
  ASSERT_EQ(catalog.at("data").size(), 1U);
  EXPECT_EQ(catalog.at("data").at(0).at("publication_id").get<std::string>(), publication_id);
  EXPECT_EQ(catalog.at("data").at(0).at("title").get<std::string>(), k_title);
  EXPECT_EQ(catalog.at("data").at(0).at("author_name").get<std::string>(), "автор");
  EXPECT_EQ(catalog.at("data").at(0).at("size").get<std::size_t>(), canonical_form(text).size());
  EXPECT_FALSE(catalog.at("data").at(0).contains("identity"));

  const auto mine =
      response_of(m_surface.author_publications(request_of(Json{{"credentials", Json::parse(author_credentials)}})));

  ASSERT_TRUE(mine.contains("data")) << mine.dump();
  ASSERT_EQ(mine.at("data").size(), 1U);
  EXPECT_EQ(mine.at("data").at(0).at("purchases").get<std::size_t>(), 0U);

  const auto device = generate_device_key();
  const auto other_device = generate_device_key();
  ASSERT_TRUE(device.has_value());
  ASSERT_TRUE(other_device.has_value());

  const auto receipt = response_of(
      m_surface.buy(request_of(Json{{"credentials", Json::parse(buyer_credentials)},
                                    {"publication_id", publication_id},
                                    {"device_key", to_hex(device->public_key.data(), device->public_key.size())}})));

  ASSERT_TRUE(receipt.contains("data")) << receipt.dump();

  const std::string purchase_id = receipt.at("data").at("header").at("purchase_id").get<std::string>();
  EXPECT_EQ(receipt.at("data").at("header").at("purchased_at").get<dgds::core::Timestamp>(), m_now);

  const auto purchased =
      response_of(m_surface.purchases(request_of(Json{{"credentials", Json::parse(buyer_credentials)}})));

  ASSERT_TRUE(purchased.contains("data")) << purchased.dump();
  ASSERT_EQ(purchased.at("data").size(), 1U);
  EXPECT_EQ(purchased.at("data").at(0).at("publication").at("publication_id").get<std::string>(), publication_id);
  EXPECT_EQ(purchased.at("data").at(0).at("publication").at("title").get<std::string>(), k_title);

  m_now += 10;

  const auto restored = response_of(m_surface.restore_receipt(
      request_of(Json{{"credentials", Json::parse(buyer_credentials)},
                      {"purchase_id", purchase_id},
                      {"device_key", to_hex(other_device->public_key.data(), other_device->public_key.size())}})));

  ASSERT_TRUE(restored.contains("data")) << restored.dump();
  EXPECT_EQ(restored.at("data").at("header").at("purchase_id").get<std::string>(), purchase_id);
  EXPECT_EQ(restored.at("data").at("header").at("issued_at").get<dgds::core::Timestamp>(), m_now);

  const auto expected = response_of(m_surface.context_identity(
      request_of(Json{{"credentials", Json::parse(buyer_credentials)}, {"context_id", purchase_id}})));

  ASSERT_TRUE(expected.contains("data")) << expected.dump();
  EXPECT_EQ(expected.at("data").get<std::string>(), to_hex(identity.value()));

  const auto fetched = response_of(m_surface.fetch_package(
      request_of(Json{{"credentials", Json::parse(buyer_credentials)},
                      {"purchase_id", purchase_id},
                      {"device_key", to_hex(device->public_key.data(), device->public_key.size())}})));

  ASSERT_TRUE(fetched.contains("data")) << fetched.dump();

  const Package package = package_of(fetched.at("data"));
  EXPECT_EQ(package.version, k_package_version);
  EXPECT_EQ(package.identity, identity.value());

  const auto receipt_key = open_receipt_key(receipt_of(receipt.at("data")), device->private_key);
  ASSERT_TRUE(receipt_key.has_value());

  const auto content = open_package(package, receipt_key.value());

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(SurfaceTest, ReportsMalformedRequestAndUnknownOperation) {
  const auto malformed = response_of(m_surface.register_user(R"({"nope": 1})"));
  ASSERT_TRUE(malformed.contains("error")) << malformed.dump();
  EXPECT_EQ(malformed.at("error").at("code").get<std::string>(),
            std::string(dgds::server::api::code_of(dgds::server::api::ProtocolError::request_malformed)));

  const auto broken = response_of(m_surface.log_in("{"));
  ASSERT_TRUE(broken.contains("error")) << broken.dump();
  EXPECT_EQ(broken.at("error").at("code").get<std::string>(), "request_malformed");

  const auto unknown = response_of(Surface::unknown_operation());
  ASSERT_TRUE(unknown.contains("error")) << unknown.dump();
  EXPECT_EQ(unknown.at("error").at("code").get<std::string>(),
            std::string(dgds::server::api::code_of(dgds::server::api::ProtocolError::operation_unknown)));
}

TEST_F(SurfaceTest, RefusesUnsupportedProtocolVersion) {
  const auto unsupported = response_of(m_surface.register_user(Json{{"name", "автор"}, {"version", 99}}.dump()));

  ASSERT_TRUE(unsupported.contains("error")) << unsupported.dump();
  EXPECT_EQ(unsupported.at("error").at("code").get<std::string>(),
            std::string(dgds::server::api::code_of(dgds::server::api::ProtocolError::version_unsupported)));

  const auto missing = response_of(m_surface.register_user(R"({"name": "автор"})"));

  ASSERT_TRUE(missing.contains("error")) << missing.dump();
  EXPECT_EQ(missing.at("error").at("code").get<std::string>(),
            std::string(dgds::server::api::code_of(dgds::server::api::ProtocolError::request_malformed)));

  const auto accepted = response_of(m_surface.register_user(request_of(Json{{"name", "автор"}})));

  ASSERT_TRUE(accepted.contains("data")) << accepted.dump();
  EXPECT_EQ(accepted.at("data").at("name").get<std::string>(), "автор");
}

TEST_F(SurfaceTest, DistinguishesErrorKinds) {
  const std::string author_account = account_of("автор");
  const std::string buyer_account = account_of("покупатель");
  const std::string author_credentials = credentials_of(author_account);
  const std::string buyer_credentials = credentials_of(buyer_account);

  const std::string text = long_text();
  const auto keys = generate_author_key();
  const auto identity = content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const Json publication{{"credentials", Json::parse(author_credentials)},
                         {"draft", Json{{"title", std::string(k_title)}, {"file_name", "файл.txt"}, {"content", text}}},
                         {"author_key", to_hex(keys->public_key.data(), keys->public_key.size())},
                         {"signature", to_hex(signature->data(), signature->size())}};

  const auto published = response_of(m_surface.publish(request_of(publication)));
  ASSERT_TRUE(published.contains("data")) << published.dump();

  const std::string publication_id = published.at("data").get<std::string>();
  const auto device = generate_device_key();
  ASSERT_TRUE(device.has_value());

  const auto duplicate = response_of(m_surface.publish(request_of(publication)));
  const auto missing_publication = response_of(
      m_surface.buy(request_of(Json{{"credentials", Json::parse(buyer_credentials)},
                                    {"publication_id", "7"},
                                    {"device_key", to_hex(device->public_key.data(), device->public_key.size())}})));

  const auto owned = response_of(
      m_surface.buy(request_of(Json{{"credentials", Json::parse(author_credentials)},
                                    {"publication_id", publication_id},
                                    {"device_key", to_hex(device->public_key.data(), device->public_key.size())}})));
  ASSERT_TRUE(owned.contains("data")) << owned.dump();

  const auto not_permitted = response_of(m_surface.context_identity(
      request_of(Json{{"credentials", Json::parse(buyer_credentials)}, {"context_id", publication_id}})));

  const auto version = response_of(m_surface.register_user(Json{{"name", "третий"}, {"version", 99}}.dump()));

  Json forged = Json::parse(buyer_credentials);
  forged["token"] = "поддельный";
  const auto authorization = response_of(m_surface.purchases(request_of(Json{{"credentials", forged}})));

  const auto operation = response_of(Surface::unknown_operation());

  const auto code_in = [](const Json &response) { return response.at("error").at("code").get<std::string>(); };

  EXPECT_EQ(code_in(duplicate), std::string(code_of(CoreError::content_duplicate)));
  EXPECT_EQ(code_in(missing_publication), std::string(code_of(CoreError::publication_not_found)));
  EXPECT_EQ(code_in(not_permitted), std::string(code_of(CoreError::not_permitted)));
  EXPECT_EQ(code_in(version), std::string(code_of(ProtocolError::version_unsupported)));
  EXPECT_EQ(code_in(authorization), std::string(code_of(CoreError::authorization_failed)));
  EXPECT_EQ(code_in(operation), std::string(code_of(ProtocolError::operation_unknown)));

  const std::set<std::string> codes{code_in(duplicate), code_in(missing_publication), code_in(not_permitted),
                                    code_in(version),   code_in(authorization),       code_in(operation)};

  EXPECT_EQ(codes.size(), 6U);
}

} // namespace
