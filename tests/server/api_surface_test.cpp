#include <dgds/server/api/errors.h>
#include <dgds/server/api/surface.h>
#include <dgds/server/middleware/rate_limiter.h>
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

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

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
using dgds::core::UserId;
using dgds::server::CatalogService;
using dgds::server::DeliveryService;
using dgds::server::LimitedOperation;
using dgds::server::PublicationService;
using dgds::server::PurchaseService;
using dgds::server::RateLimit;
using dgds::server::RateLimiter;
using dgds::server::SessionStore;
using dgds::server::UserService;
using dgds::server::api::code_of;
using dgds::server::api::ProtocolError;
using dgds::server::api::Surface;
using dgds::server::api::SurfaceResult;
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

  [[nodiscard]] Json response_of(const SurfaceResult &result) { return response_of(result.body); }

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

  [[nodiscard]] std::string publish_text(const std::string &credentials, std::string_view name,
                                         const std::string &text) {
    const auto keys = generate_author_key();
    const auto identity = content_identity(text);
    EXPECT_TRUE(keys.has_value());
    EXPECT_TRUE(identity.has_value());

    const auto signature = sign_author(identity.value(), name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    const auto published = response_of(m_surface.publish(
        request_of(Json{{"credentials", Json::parse(credentials)},
                        {"draft", Json{{"title", std::string(k_title)}, {"file_name", "файл.txt"}, {"content", text}}},
                        {"author_key", to_hex(keys->public_key.data(), keys->public_key.size())},
                        {"signature", to_hex(signature->data(), signature->size())}})));
    EXPECT_TRUE(published.contains("data")) << published.dump();

    return published.contains("data") ? published.at("data").get<std::string>() : std::string{};
  }

  [[nodiscard]] std::string buy_publication(const std::string &credentials, const std::string &publication_id) {
    const auto device = generate_device_key();
    EXPECT_TRUE(device.has_value());

    const auto receipt = response_of(
        m_surface.buy(request_of(Json{{"credentials", Json::parse(credentials)},
                                      {"publication_id", publication_id},
                                      {"device_key", to_hex(device->public_key.data(), device->public_key.size())}})));
    EXPECT_TRUE(receipt.contains("data")) << receipt.dump();

    return receipt.contains("data") ? receipt.at("data").at("header").at("purchase_id").get<std::string>()
                                    : std::string{};
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
  RateLimiter m_limiter;
  Surface m_surface{m_users,     m_sessions, m_catalog, m_publications,
                    m_purchases, m_delivery, m_limiter, [this]() { return m_now; }};

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(SurfaceTest, RestartsWindowWhenClockMovesBack) {
  RateLimiter limiter{RateLimit{.calls = 2, .window = 60}};
  UserId user{};
  user[0] = 1;

  ASSERT_TRUE(limiter.accepted(LimitedOperation::publication, user, 1000));
  ASSERT_TRUE(limiter.accepted(LimitedOperation::publication, user, 1001));
  ASSERT_FALSE(limiter.accepted(LimitedOperation::publication, user, 1002));

  EXPECT_TRUE(limiter.accepted(LimitedOperation::publication, user, 500))
      << "обратный ход часов должен открывать окно заново";
  EXPECT_TRUE(limiter.accepted(LimitedOperation::publication, user, 5000))
      << "прямой ход часов открывает окно досрочно";
}

TEST_F(SurfaceTest, RejectsNameOutsideAllowedLength) {
  const auto too_long = response_of(m_surface.register_user(request_of(Json{{"name", std::string(257, 'x')}})));
  EXPECT_EQ(too_long.at("error").at("code").get<std::string>(), "request_malformed");

  const auto empty = response_of(m_surface.register_user(request_of(Json{{"name", ""}})));
  EXPECT_EQ(empty.at("error").at("code").get<std::string>(), "request_malformed");
}

TEST_F(SurfaceTest, RejectsDraftFieldOutsideAllowedLength) {
  const std::string credentials = credentials_of(account_of("автор"));

  const Json request{{"credentials", Json::parse(credentials)},
                     {"draft", Json{{"title", std::string(257, 'x')}, {"file_name", "файл.txt"}, {"content", "текст"}}},
                     {"author_key", std::string(64, 'a')},
                     {"signature", std::string(128, 'b')}};

  const auto rejected = response_of(m_surface.publish(request_of(request)));

  EXPECT_EQ(rejected.at("error").at("code").get<std::string>(), "request_malformed");
}

TEST_F(SurfaceTest, RejectsContentLargerThanLimit) {
  const std::string credentials = credentials_of(account_of("автор"));
  const std::string content(dgds::core::k_max_content_bytes + 1, 'x');

  const auto keys = generate_author_key();
  ASSERT_TRUE(keys.has_value());

  const auto identity = dgds::core::content_identity(content);
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const Json request{{"credentials", Json::parse(credentials)},
                     {"draft", Json{{"title", std::string(k_title)}, {"file_name", "файл.txt"}, {"content", content}}},
                     {"author_key", to_hex(keys->public_key.data(), keys->public_key.size())},
                     {"signature", to_hex(signature->data(), signature->size())}};

  const auto rejected = response_of(m_surface.publish(request_of(request)));

  EXPECT_TRUE(rejected.contains("error")) << rejected.dump();

  if (rejected.contains("error")) {
    EXPECT_EQ(rejected.at("error").at("code").get<std::string>(), "request_malformed");
  }

  EXPECT_FALSE(std::filesystem::exists(m_root / "blobs")) << "появился шифротекст";
  EXPECT_FALSE(std::filesystem::exists(m_root / "keys")) << "появился ключ файла";
  EXPECT_FALSE(std::filesystem::exists(m_root / "master.key")) << "появился мастер-ключ";
  EXPECT_FALSE(std::filesystem::exists(m_root / "identities")) << "идентификатор контента оказался занят";

  const auto publications = m_metadata.publications(0, dgds::core::k_default_page_size);

  ASSERT_TRUE(publications.has_value());
  EXPECT_TRUE(publications->records.empty()) << "появилась запись публикации";
}

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
  EXPECT_EQ(catalog.at("data").at("offset").get<std::string>(), "0");
  EXPECT_EQ(catalog.at("data").at("limit").get<std::string>(), std::to_string(dgds::core::k_default_page_size));
  EXPECT_EQ(catalog.at("data").at("total").get<std::string>(), "1");
  ASSERT_EQ(catalog.at("data").at("items").size(), 1U);
  EXPECT_EQ(catalog.at("data").at("items").at(0).at("publication_id").get<std::string>(), publication_id);
  EXPECT_EQ(catalog.at("data").at("items").at(0).at("title").get<std::string>(), k_title);
  EXPECT_EQ(catalog.at("data").at("items").at(0).at("author_name").get<std::string>(), "автор");
  EXPECT_EQ(catalog.at("data").at("items").at(0).at("size").get<std::size_t>(), canonical_form(text).size());
  EXPECT_FALSE(catalog.at("data").at("items").at(0).contains("identity"));

  const auto mine =
      response_of(m_surface.author_publications(request_of(Json{{"credentials", Json::parse(author_credentials)}})));

  ASSERT_TRUE(mine.contains("data")) << mine.dump();
  EXPECT_EQ(mine.at("data").at("total").get<std::string>(), "1");
  ASSERT_EQ(mine.at("data").at("items").size(), 1U);
  EXPECT_EQ(mine.at("data").at("items").at(0).at("publication").at("publication_id").get<std::string>(),
            publication_id);
  EXPECT_EQ(mine.at("data").at("items").at(0).at("purchases").get<std::size_t>(), 0U);

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
  EXPECT_EQ(purchased.at("data").at("total").get<std::string>(), "1");
  ASSERT_EQ(purchased.at("data").at("items").size(), 1U);
  EXPECT_EQ(purchased.at("data").at("items").at(0).at("publication").at("publication_id").get<std::string>(),
            publication_id);
  EXPECT_EQ(purchased.at("data").at("items").at(0).at("publication").at("title").get<std::string>(), k_title);

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

TEST_F(SurfaceTest, RefusesExcessPublications) {
  RateLimiter limiter{RateLimit{.calls = 2, .window = 60}};
  Surface surface{m_users,     m_sessions, m_catalog, m_publications,
                  m_purchases, m_delivery, limiter,   [this]() { return m_now; }};

  const std::string author_credentials = credentials_of(account_of("автор"));
  const std::string buyer_credentials = credentials_of(account_of("покупатель"));

  const auto keys = generate_author_key();
  ASSERT_TRUE(keys.has_value());

  const auto publish_with = [&](const std::string &credentials, std::string_view name, const std::string &text) {
    const auto identity = content_identity(text);
    EXPECT_TRUE(identity.has_value());

    const auto signature = sign_author(identity.value(), name, keys->private_key);
    EXPECT_TRUE(signature.has_value());

    return response_of(surface.publish(
        request_of(Json{{"credentials", Json::parse(credentials)},
                        {"draft", Json{{"title", std::string(k_title)}, {"file_name", "файл.txt"}, {"content", text}}},
                        {"author_key", to_hex(keys->public_key.data(), keys->public_key.size())},
                        {"signature", to_hex(signature->data(), signature->size())}})));
  };

  const std::string base = long_text();

  EXPECT_TRUE(publish_with(author_credentials, "автор", base + "первый\n").contains("data"));
  EXPECT_TRUE(publish_with(author_credentials, "автор", base + "второй\n").contains("data"));

  const auto limited = publish_with(author_credentials, "автор", base + "третий\n");

  ASSERT_TRUE(limited.contains("error")) << limited.dump();
  EXPECT_EQ(limited.at("error").at("code").get<std::string>(),
            std::string(code_of(ProtocolError::rate_limit_exceeded)));

  EXPECT_TRUE(publish_with(buyer_credentials, "покупатель", base + "четвёртый\n").contains("data"));
}

TEST_F(SurfaceTest, RefusesExcessDeliveries) {
  RateLimiter limiter{RateLimit{.calls = 2, .window = 60}};
  Surface surface{m_users,     m_sessions, m_catalog, m_publications,
                  m_purchases, m_delivery, limiter,   [this]() { return m_now; }};

  const std::string author_credentials = credentials_of(account_of("автор"));
  const std::string buyer_credentials = credentials_of(account_of("покупатель"));

  const std::string text = long_text();
  const auto keys = generate_author_key();
  const auto identity = content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const auto published = response_of(surface.publish(
      request_of(Json{{"credentials", Json::parse(author_credentials)},
                      {"draft", Json{{"title", std::string(k_title)}, {"file_name", "файл.txt"}, {"content", text}}},
                      {"author_key", to_hex(keys->public_key.data(), keys->public_key.size())},
                      {"signature", to_hex(signature->data(), signature->size())}})));
  ASSERT_TRUE(published.contains("data")) << published.dump();

  const auto device = generate_device_key();
  ASSERT_TRUE(device.has_value());

  const std::string publication_id = published.at("data").get<std::string>();
  const std::string device_key = to_hex(device->public_key.data(), device->public_key.size());

  const auto receipt = response_of(surface.buy(request_of(Json{{"credentials", Json::parse(buyer_credentials)},
                                                               {"publication_id", publication_id},
                                                               {"device_key", device_key}})));
  ASSERT_TRUE(receipt.contains("data")) << receipt.dump();

  const std::string purchase_id = receipt.at("data").at("header").at("purchase_id").get<std::string>();

  const auto fetch = [&] {
    return response_of(surface.fetch_package(request_of(Json{
        {"credentials", Json::parse(buyer_credentials)}, {"purchase_id", purchase_id}, {"device_key", device_key}})));
  };

  EXPECT_TRUE(fetch().contains("data"));
  EXPECT_TRUE(fetch().contains("data"));

  const auto limited = fetch();

  ASSERT_TRUE(limited.contains("error")) << limited.dump();
  EXPECT_EQ(limited.at("error").at("code").get<std::string>(),
            std::string(code_of(ProtocolError::rate_limit_exceeded)));

  const auto other_device = generate_device_key();
  ASSERT_TRUE(other_device.has_value());

  const std::string other_key = to_hex(other_device->public_key.data(), other_device->public_key.size());

  const auto own_receipt = response_of(surface.buy(request_of(Json{{"credentials", Json::parse(author_credentials)},
                                                                   {"publication_id", publication_id},
                                                                   {"device_key", other_key}})));
  ASSERT_TRUE(own_receipt.contains("data")) << own_receipt.dump();

  const auto other_fetch = response_of(surface.fetch_package(request_of(Json{
      {"credentials", Json::parse(author_credentials)}, {"purchase_id", publication_id}, {"device_key", other_key}})));

  ASSERT_TRUE(other_fetch.contains("data")) << other_fetch.dump();
}

void expect_windows_cover_list(const std::function<Json(const Json &)> &list, std::size_t limit) {
  const Json whole = list(Json::object());

  ASSERT_TRUE(whole.contains("data")) << whole.dump();

  const Json &all_items = whole.at("data").at("items");
  ASSERT_FALSE(all_items.empty()) << whole.dump();

  std::vector<std::string> expected;

  for (const Json &entry : all_items) {
    expected.push_back(entry.dump());
  }

  std::vector<std::string> walked;

  for (std::size_t offset = 0; offset <= all_items.size(); offset += limit) {
    const Json page = list(Json{{"offset", std::to_string(offset)}, {"limit", std::to_string(limit)}});

    ASSERT_TRUE(page.contains("data")) << page.dump();
    EXPECT_EQ(page.at("data").at("offset").get<std::string>(), std::to_string(offset));
    EXPECT_EQ(page.at("data").at("limit").get<std::string>(), std::to_string(limit));
    EXPECT_EQ(page.at("data").at("total").get<std::string>(), std::to_string(all_items.size()));
    EXPECT_LE(page.at("data").at("items").size(), limit);

    for (const Json &entry : page.at("data").at("items")) {
      walked.push_back(entry.dump());
    }
  }

  ASSERT_EQ(walked.size(), expected.size()) << "обход окнами не покрыл список";

  for (std::size_t index = 0; index < expected.size(); ++index) {
    EXPECT_EQ(walked[index], expected[index]) << "окна не состыковались на записи " << index;
  }
}

TEST_F(SurfaceTest, AppliesDefaultWindowWhenFieldsAbsent) {
  const std::string author_credentials = credentials_of(account_of("автор"));
  const std::string buyer_credentials = credentials_of(account_of("покупатель"));

  const std::string first = publish_text(author_credentials, "автор", long_text() + "первая строка\n");
  EXPECT_FALSE(publish_text(author_credentials, "автор", long_text() + "вторая строка\n").empty());
  EXPECT_FALSE(buy_publication(buyer_credentials, first).empty());

  const std::string default_limit = std::to_string(dgds::core::k_default_page_size);

  const auto catalog = response_of(m_surface.catalog(request_of(Json::object())));

  ASSERT_TRUE(catalog.contains("data")) << catalog.dump();
  EXPECT_EQ(catalog.at("data").at("offset").get<std::string>(), "0");
  EXPECT_EQ(catalog.at("data").at("limit").get<std::string>(), default_limit);
  EXPECT_EQ(catalog.at("data").at("total").get<std::string>(), "2");
  EXPECT_EQ(catalog.at("data").at("items").size(), 2U);

  const auto only_limit = response_of(m_surface.catalog(request_of(Json{{"limit", "1"}})));

  ASSERT_TRUE(only_limit.contains("data")) << only_limit.dump();
  EXPECT_EQ(only_limit.at("data").at("offset").get<std::string>(), "0");
  EXPECT_EQ(only_limit.at("data").at("limit").get<std::string>(), "1");
  EXPECT_EQ(only_limit.at("data").at("total").get<std::string>(), "2");
  ASSERT_EQ(only_limit.at("data").at("items").size(), 1U);

  const auto only_offset = response_of(m_surface.catalog(request_of(Json{{"offset", "1"}})));

  ASSERT_TRUE(only_offset.contains("data")) << only_offset.dump();
  EXPECT_EQ(only_offset.at("data").at("offset").get<std::string>(), "1");
  EXPECT_EQ(only_offset.at("data").at("limit").get<std::string>(), default_limit);
  EXPECT_EQ(only_offset.at("data").at("total").get<std::string>(), "2");
  ASSERT_EQ(only_offset.at("data").at("items").size(), 1U);

  const auto mine =
      response_of(m_surface.author_publications(request_of(Json{{"credentials", Json::parse(author_credentials)}})));

  ASSERT_TRUE(mine.contains("data")) << mine.dump();
  EXPECT_EQ(mine.at("data").at("offset").get<std::string>(), "0");
  EXPECT_EQ(mine.at("data").at("limit").get<std::string>(), default_limit);
  EXPECT_EQ(mine.at("data").at("total").get<std::string>(), "2");
  EXPECT_EQ(mine.at("data").at("items").size(), 2U);

  const auto purchased =
      response_of(m_surface.purchases(request_of(Json{{"credentials", Json::parse(buyer_credentials)}})));

  ASSERT_TRUE(purchased.contains("data")) << purchased.dump();
  EXPECT_EQ(purchased.at("data").at("offset").get<std::string>(), "0");
  EXPECT_EQ(purchased.at("data").at("limit").get<std::string>(), default_limit);
  EXPECT_EQ(purchased.at("data").at("total").get<std::string>(), "1");
  EXPECT_EQ(purchased.at("data").at("items").size(), 1U);
}

TEST_F(SurfaceTest, ReturnsRequestedWindowForEveryList) {
  const std::string author_credentials = credentials_of(account_of("автор"));
  const std::string buyer_credentials = credentials_of(account_of("покупатель"));

  std::vector<std::string> purchases;

  for (std::size_t index = 0; index < 3; ++index) {
    const std::string publication_id =
        publish_text(author_credentials, "автор", long_text() + std::to_string(index) + " строка\n");
    purchases.push_back(buy_publication(buyer_credentials, publication_id));
  }

  const auto whole = response_of(m_surface.catalog(request_of(Json::object())));

  ASSERT_TRUE(whole.contains("data")) << whole.dump();
  ASSERT_EQ(whole.at("data").at("items").size(), 3U);

  const auto repeated = response_of(m_surface.catalog(request_of(Json::object())));

  ASSERT_TRUE(repeated.contains("data")) << repeated.dump();
  EXPECT_EQ(whole.at("data").at("items").dump(), repeated.at("data").at("items").dump())
      << "порядок каталога должен быть устойчивым";

  std::uint64_t previous = 0;

  for (const Json &entry : whole.at("data").at("items")) {
    const std::uint64_t identifier = std::stoull(entry.at("publication_id").get<std::string>());
    EXPECT_LT(previous, identifier) << "каталог должен быть упорядочен по идентификатору";
    previous = identifier;
  }

  const auto window = response_of(m_surface.catalog(request_of(Json{{"offset", "1"}, {"limit", "1"}})));

  ASSERT_TRUE(window.contains("data")) << window.dump();
  EXPECT_EQ(window.at("data").at("offset").get<std::string>(), "1");
  EXPECT_EQ(window.at("data").at("limit").get<std::string>(), "1");
  EXPECT_EQ(window.at("data").at("total").get<std::string>(), "3");
  ASSERT_EQ(window.at("data").at("items").size(), 1U);
  EXPECT_EQ(window.at("data").at("items").at(0).dump(), whole.at("data").at("items").at(1).dump())
      << "окно должно отдавать ту же запись, что и полный список";

  const auto mine = response_of(m_surface.author_publications(
      request_of(Json{{"credentials", Json::parse(author_credentials)}, {"offset", "2"}, {"limit", "1"}})));

  ASSERT_TRUE(mine.contains("data")) << mine.dump();
  EXPECT_EQ(mine.at("data").at("total").get<std::string>(), "3");
  ASSERT_EQ(mine.at("data").at("items").size(), 1U);
  EXPECT_EQ(mine.at("data").at("items").at(0).at("publication").dump(), whole.at("data").at("items").at(2).dump())
      << "окно авторского списка должно отдавать ту же запись, что и каталог";

  std::sort(purchases.begin(), purchases.end(),
            [](const std::string &left, const std::string &right) { return std::stoull(left) < std::stoull(right); });

  const auto purchased = response_of(m_surface.purchases(
      request_of(Json{{"credentials", Json::parse(buyer_credentials)}, {"offset", "1"}, {"limit", "2"}})));

  ASSERT_TRUE(purchased.contains("data")) << purchased.dump();
  EXPECT_EQ(purchased.at("data").at("offset").get<std::string>(), "1");
  EXPECT_EQ(purchased.at("data").at("limit").get<std::string>(), "2");
  EXPECT_EQ(purchased.at("data").at("total").get<std::string>(), "3");
  ASSERT_EQ(purchased.at("data").at("items").size(), 2U);
  EXPECT_EQ(purchased.at("data").at("items").at(0).at("purchase_id").get<std::string>(), purchases[1]);
  EXPECT_EQ(purchased.at("data").at("items").at(1).at("purchase_id").get<std::string>(), purchases[2]);
}

TEST_F(SurfaceTest, WalksEveryListInWindowsWithoutRepeats) {
  const std::string author_credentials = credentials_of(account_of("автор"));
  const std::string buyer_credentials = credentials_of(account_of("покупатель"));

  for (std::size_t index = 0; index < 5; ++index) {
    const std::string publication_id =
        publish_text(author_credentials, "автор", long_text() + std::to_string(index) + " строка\n");
    EXPECT_FALSE(buy_publication(buyer_credentials, publication_id).empty());
  }

  expect_windows_cover_list([this](const Json &window) { return response_of(m_surface.catalog(request_of(window))); },
                            2);

  expect_windows_cover_list(
      [this, &author_credentials](const Json &window) {
        Json body = window;
        body["credentials"] = Json::parse(author_credentials);

        return response_of(m_surface.author_publications(request_of(body)));
      },
      2);

  expect_windows_cover_list(
      [this, &buyer_credentials](const Json &window) {
        Json body = window;
        body["credentials"] = Json::parse(buyer_credentials);

        return response_of(m_surface.purchases(request_of(body)));
      },
      2);
}

TEST_F(SurfaceTest, ReturnsEmptyWindowBeyondLastRecord) {
  const std::string author_credentials = credentials_of(account_of("автор"));
  const std::string buyer_credentials = credentials_of(account_of("покупатель"));

  const std::string publication_id = publish_text(author_credentials, "автор", long_text());
  EXPECT_FALSE(buy_publication(buyer_credentials, publication_id).empty());

  const auto catalog = response_of(m_surface.catalog(request_of(Json{{"offset", "9"}, {"limit", "2"}})));

  ASSERT_TRUE(catalog.contains("data")) << catalog.dump();
  EXPECT_EQ(catalog.at("data").at("offset").get<std::string>(), "9");
  EXPECT_EQ(catalog.at("data").at("total").get<std::string>(), "1");
  EXPECT_TRUE(catalog.at("data").at("items").empty());

  const auto mine = response_of(m_surface.author_publications(
      request_of(Json{{"credentials", Json::parse(author_credentials)}, {"offset", "9"}, {"limit", "2"}})));

  ASSERT_TRUE(mine.contains("data")) << mine.dump();
  EXPECT_EQ(mine.at("data").at("total").get<std::string>(), "1");
  EXPECT_TRUE(mine.at("data").at("items").empty());

  const auto purchased = response_of(m_surface.purchases(
      request_of(Json{{"credentials", Json::parse(buyer_credentials)}, {"offset", "9"}, {"limit", "2"}})));

  ASSERT_TRUE(purchased.contains("data")) << purchased.dump();
  EXPECT_EQ(purchased.at("data").at("total").get<std::string>(), "1");
  EXPECT_TRUE(purchased.at("data").at("items").empty());

  const auto empty_window = response_of(m_surface.catalog(request_of(Json{{"offset", "0"}, {"limit", "0"}})));

  ASSERT_TRUE(empty_window.contains("data")) << empty_window.dump();
  EXPECT_EQ(empty_window.at("data").at("limit").get<std::string>(), "0");
  EXPECT_EQ(empty_window.at("data").at("total").get<std::string>(), "1");
  EXPECT_TRUE(empty_window.at("data").at("items").empty());
}

TEST_F(SurfaceTest, RejectsWindowValuesOutsideAllowedRange) {
  const std::string author_credentials = credentials_of(account_of("автор"));
  const std::string buyer_credentials = credentials_of(account_of("покупатель"));

  const std::vector<std::function<SurfaceResult(const Json &)>> lists{
      [this](const Json &body) { return m_surface.catalog(request_of(body)); },
      [this](const Json &body) { return m_surface.author_publications(request_of(body)); },
      [this](const Json &body) { return m_surface.purchases(request_of(body)); }};

  const std::vector<Json> basics{Json::object(), Json{{"credentials", Json::parse(author_credentials)}},
                                 Json{{"credentials", Json::parse(buyer_credentials)}}};

  const std::vector<Json> windows{Json{{"limit", std::to_string(dgds::core::k_max_page_size + 1)}},
                                  Json{{"limit", "-1"}},
                                  Json{{"limit", "нет"}},
                                  Json{{"limit", 5}},
                                  Json{{"offset", "-1"}},
                                  Json{{"offset", "нет"}}};

  for (std::size_t index = 0; index < lists.size(); ++index) {
    Json body = basics[index];

    for (const Json &window : windows) {
      for (const auto &[field, value] : window.items()) {
        body[field] = value;
      }

      const auto rejected = response_of(lists[index](body));

      ASSERT_TRUE(rejected.contains("error")) << rejected.dump();
      EXPECT_EQ(rejected.at("error").at("code").get<std::string>(), "request_malformed") << rejected.dump();

      body.erase("offset");
      body.erase("limit");
    }
  }
}

} // namespace
