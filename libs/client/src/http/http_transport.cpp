#include <dgds/client/http/http_transport.h>

#include <dgds/client/http/pinned_client.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/user_id.h>
#include <dgds/core/models/protocol.h>

#include <nlohmann/json.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace dgds::client {
namespace {

using Json = nlohmann::json;

constexpr int k_connect_timeout_seconds = 5;
constexpr int k_read_timeout_seconds = 30;

using Bytes = std::vector<std::uint8_t>;

[[nodiscard]] core::Result<Json> field(const Json &body, std::string_view name) {
  const auto found = body.find(std::string(name));

  if (!body.is_object() || found == body.end()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return *found;
}

[[nodiscard]] core::Result<std::string> text_of(const Json &body, std::string_view name) {
  const auto value = field(body, name);

  if (!value.has_value() || !value->is_string()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return value->get<std::string>();
}

[[nodiscard]] core::Result<std::uint64_t> number_of(const Json &body, std::string_view name) {
  const auto value = field(body, name);

  if (!value.has_value() || !value->is_number_unsigned()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return value->get<std::uint64_t>();
}

[[nodiscard]] core::Result<std::uint64_t> identifier_of_text(const std::string &text) {
  std::uint64_t value = 0;
  const char *begin = text.data();
  const char *end = begin + text.size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);

  if (parsed.ec != std::errc() || parsed.ptr != end) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return value;
}

[[nodiscard]] core::Result<std::uint64_t> identifier_of(const Json &body, std::string_view name) {
  const auto text = text_of(body, name);

  if (!text.has_value()) {
    return std::unexpected(text.error());
  }

  return identifier_of_text(text.value());
}

[[nodiscard]] std::optional<Bytes> decoded_of(const std::string &text) {
  const auto decoded = core::from_hex(text);

  if (!decoded.has_value()) {
    return std::nullopt;
  }

  return Bytes(decoded->begin(), decoded->end());
}

template <std::size_t Size>
[[nodiscard]] core::Result<std::array<std::uint8_t, Size>> bytes_of(const Json &body, std::string_view name) {
  const auto text = text_of(body, name);

  if (!text.has_value()) {
    return std::unexpected(text.error());
  }

  const auto decoded = decoded_of(text.value());

  if (!decoded.has_value() || decoded->size() != Size) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  std::array<std::uint8_t, Size> bytes{};
  std::memcpy(bytes.data(), decoded->data(), Size);

  return bytes;
}

[[nodiscard]] core::Result<core::Ciphertext> ciphertext_of(const Json &body, std::string_view name) {
  const auto text = text_of(body, name);

  if (!text.has_value()) {
    return std::unexpected(text.error());
  }

  const auto decoded = decoded_of(text.value());

  if (!decoded.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return decoded.value();
}

[[nodiscard]] core::Result<core::SealedContent> sealed_of(const Json &body) {
  const auto algorithm = number_of(body, "algorithm");
  const auto nonce = bytes_of<core::k_nonce_size>(body, "nonce");
  const auto ciphertext = ciphertext_of(body, "ciphertext");
  const auto tag = bytes_of<core::k_tag_size>(body, "tag");

  if (!algorithm.has_value() || !nonce.has_value() || !ciphertext.has_value() || !tag.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  const auto named = static_cast<core::AeadAlgorithm>(algorithm.value());

  if (named != core::AeadAlgorithm::aes_256_gcm) {
    return std::unexpected(core::CoreError::algorithm_unsupported);
  }

  return core::SealedContent{
      .algorithm = named, .nonce = nonce.value(), .ciphertext = ciphertext.value(), .tag = tag.value()};
}

[[nodiscard]] core::Result<core::DeviceEnvelope> device_envelope_of(const Json &body) {
  const auto ephemeral = bytes_of<core::k_device_key_size>(body, "ephemeral_key");
  const auto wrapped = field(body, "wrapped");

  if (!ephemeral.has_value() || !wrapped.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  const auto sealed = sealed_of(wrapped.value());

  if (!sealed.has_value()) {
    return std::unexpected(sealed.error());
  }

  return core::DeviceEnvelope{.ephemeral_key = ephemeral.value(), .wrapped = sealed.value()};
}

[[nodiscard]] core::Result<core::UserId> user_id_of(const Json &body) {
  const auto text = text_of(body, "user_id");

  if (!text.has_value()) {
    return std::unexpected(text.error());
  }

  const auto parsed = core::from_uuid(text.value());

  if (!parsed.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return parsed.value();
}

[[nodiscard]] core::Result<core::ContentIdentity> identity_of_text(const std::string &text) {
  const auto decoded = decoded_of(text);

  if (!decoded.has_value() || decoded->size() != core::k_identity_size) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  core::ContentIdentity identity{};
  std::memcpy(identity.data(), decoded->data(), core::k_identity_size);

  return identity;
}

[[nodiscard]] core::Result<core::ContentIdentity> identity_of(const Json &body, std::string_view name) {
  const auto text = text_of(body, name);

  if (!text.has_value()) {
    return std::unexpected(text.error());
  }

  return identity_of_text(text.value());
}

[[nodiscard]] core::Result<core::UserAccount> account_of(const Json &body) {
  const auto user_id = user_id_of(body);
  const auto name = text_of(body, "name");

  if (!user_id.has_value() || !name.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return core::UserAccount{.user_id = user_id.value(), .name = name.value()};
}

[[nodiscard]] core::Result<core::Credentials> credentials_of(const Json &body) {
  const auto user_id = user_id_of(body);
  const auto token = text_of(body, "token");

  if (!user_id.has_value() || !token.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return core::Credentials{.user_id = user_id.value(), .token = token.value()};
}

[[nodiscard]] core::Result<core::PublicationSummary> summary_of(const Json &body) {
  const auto publication_id = identifier_of(body, "publication_id");
  const auto title = text_of(body, "title");
  const auto file_name = text_of(body, "file_name");
  const auto size = number_of(body, "size");
  const auto published_at = number_of(body, "published_at");
  const auto author_name = text_of(body, "author_name");

  if (!publication_id.has_value() || !title.has_value() || !file_name.has_value() || !size.has_value() ||
      !published_at.has_value() || !author_name.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return core::PublicationSummary{.publication_id = publication_id.value(),
                                  .title = title.value(),
                                  .file_name = file_name.value(),
                                  .size = static_cast<std::size_t>(size.value()),
                                  .published_at = static_cast<core::Timestamp>(published_at.value()),
                                  .author_name = author_name.value()};
}

[[nodiscard]] core::Result<core::PurchaseSummary> purchase_summary_of(const Json &body) {
  const auto purchase_id = identifier_of(body, "purchase_id");
  const auto publication = field(body, "publication");
  const auto purchased_at = number_of(body, "purchased_at");

  if (!purchase_id.has_value() || !publication.has_value() || !purchased_at.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  const auto summary = summary_of(publication.value());

  if (!summary.has_value()) {
    return std::unexpected(summary.error());
  }

  return core::PurchaseSummary{.purchase_id = purchase_id.value(),
                               .publication = summary.value(),
                               .purchased_at = static_cast<core::Timestamp>(purchased_at.value())};
}

[[nodiscard]] core::Result<core::Receipt> receipt_of(const Json &body) {
  const auto header = field(body, "header");
  const auto wrapped = field(body, "wrapped_key");

  if (!header.has_value() || !wrapped.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  const auto version = number_of(header.value(), "version");
  const auto purchase_id = identifier_of(header.value(), "purchase_id");
  const auto user_id = user_id_of(header.value());
  const auto purchased_at = number_of(header.value(), "purchased_at");
  const auto issued_at = number_of(header.value(), "issued_at");

  if (!version.has_value() || !purchase_id.has_value() || !user_id.has_value() || !purchased_at.has_value() ||
      !issued_at.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  if (version.value() != core::k_receipt_version) {
    return std::unexpected(core::CoreError::receipt_version_unsupported);
  }

  const auto envelope = device_envelope_of(wrapped.value());

  if (!envelope.has_value()) {
    return std::unexpected(envelope.error());
  }

  return core::Receipt{.header = core::ReceiptHeader{.version = static_cast<std::uint8_t>(version.value()),
                                                     .purchase_id = purchase_id.value(),
                                                     .user_id = user_id.value(),
                                                     .purchased_at = static_cast<core::Timestamp>(purchased_at.value()),
                                                     .issued_at = static_cast<core::Timestamp>(issued_at.value())},
                       .wrapped_key = envelope.value()};
}

[[nodiscard]] core::Result<core::Package> package_of(const Json &body) {
  const auto version = number_of(body, "version");
  const auto content = field(body, "content");
  const auto wrapped_blob_key = field(body, "wrapped_blob_key");
  const auto identity = identity_of(body, "identity");
  const auto author_key = bytes_of<core::k_author_public_key_size>(body, "author_key");
  const auto author_name = text_of(body, "author_name");
  const auto signature_algorithm = number_of(body, "signature_algorithm");
  const auto signature = bytes_of<core::k_signature_size>(body, "signature");

  if (!version.has_value() || !content.has_value() || !wrapped_blob_key.has_value() || !identity.has_value() ||
      !author_key.has_value() || !author_name.has_value() || !signature_algorithm.has_value() ||
      !signature.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  if (version.value() != core::k_package_version) {
    return std::unexpected(core::CoreError::package_version_unsupported);
  }

  const auto named = static_cast<core::SignatureAlgorithm>(signature_algorithm.value());

  if (named != core::SignatureAlgorithm::ed25519) {
    return std::unexpected(core::CoreError::algorithm_unsupported);
  }

  const auto sealed_content = sealed_of(content.value());
  const auto sealed_key = sealed_of(wrapped_blob_key.value());

  if (!sealed_content.has_value() || !sealed_key.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return core::Package{.version = static_cast<std::uint8_t>(version.value()),
                       .content = sealed_content.value(),
                       .wrapped_blob_key = sealed_key.value(),
                       .identity = identity.value(),
                       .author_key = author_key.value(),
                       .author_name = author_name.value(),
                       .signature_algorithm = named,
                       .signature = signature.value()};
}

[[nodiscard]] core::Result<Json> exchange(const Endpoint &endpoint, const ServerTrust &trust, std::string_view path,
                                          Json body) {
  body["version"] = core::k_protocol_version;

  const auto client = make_pinned_client(endpoint, trust);

  if (!client.has_value()) {
    return std::unexpected(client.error());
  }

  (*client)->set_connection_timeout(k_connect_timeout_seconds);
  (*client)->set_read_timeout(k_read_timeout_seconds);

  const httplib::Result response = (*client)->Post(std::string(path), body.dump(), "application/json");

  if (!response) {
    return std::unexpected(core::CoreError::connection_failed);
  }

  const Json parsed = Json::parse(response->body, nullptr, false);

  if (parsed.is_discarded() || !parsed.is_object()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  const auto version = number_of(parsed, "version");

  if (!version.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  if (version.value() != core::k_protocol_version) {
    return std::unexpected(core::CoreError::protocol_version_unsupported);
  }

  const auto fault = field(parsed, "error");

  if (fault.has_value()) {
    const auto code = text_of(fault.value(), "code");

    if (!code.has_value()) {
      return std::unexpected(core::CoreError::protocol_failure);
    }

    return std::unexpected(core::error_of_code(code.value()).value_or(core::CoreError::protocol_failure));
  }

  const auto data = field(parsed, "data");

  if (!data.has_value()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return data.value();
}

[[nodiscard]] Json credentials_json(const Credentials &credentials) {
  return Json{{"user_id", core::to_uuid(credentials.user_id)}, {"token", credentials.token}};
}

template <std::size_t Size> Json hex_json(const std::array<std::uint8_t, Size> &bytes) {
  return core::to_hex(bytes.data(), bytes.size());
}

[[nodiscard]] core::Result<std::vector<core::PublicationSummary>> summaries_of(const Json &data) {
  if (!data.is_array()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  std::vector<core::PublicationSummary> summaries;

  for (const Json &entry : data) {
    const auto summary = summary_of(entry);

    if (!summary.has_value()) {
      return std::unexpected(summary.error());
    }

    summaries.push_back(summary.value());
  }

  return summaries;
}

} // namespace

Result<UserAccount> HttpTransport::register_user(Content name) {
  const auto data = exchange(m_endpoint, m_trust, "/register", Json{{"name", std::string(name)}});

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  return account_of(data.value());
}

Result<Credentials> HttpTransport::log_in(Content name) {
  const auto data = exchange(m_endpoint, m_trust, "/login", Json{{"name", std::string(name)}});

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  return credentials_of(data.value());
}

Result<PublicationSummaries> HttpTransport::catalog(std::size_t offset, std::size_t limit) {
  const Json body{{"offset", std::to_string(offset)}, {"limit", std::to_string(limit)}};
  const auto data = exchange(m_endpoint, m_trust, "/catalog", body);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  return summaries_of(data.value());
}

Result<PublicationId> HttpTransport::publish(const Credentials &credentials, const PublicationDraft &draft,
                                             const AuthorPublicKey &author_key, const Signature &signature) {
  const Json body{{"credentials", credentials_json(credentials)},
                  {"draft", Json{{"title", draft.title}, {"file_name", draft.file_name}, {"content", draft.content}}},
                  {"author_key", hex_json(author_key)},
                  {"signature", hex_json(signature)}};

  const auto data = exchange(m_endpoint, m_trust, "/publish", body);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  if (!data->is_string()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return identifier_of_text(data->get<std::string>());
}

Result<Receipt> HttpTransport::buy(const Credentials &credentials, const PublicationId &publication_id,
                                   const DevicePublicKey &device_key) {
  const Json body{{"credentials", credentials_json(credentials)},
                  {"publication_id", std::to_string(publication_id)},
                  {"device_key", hex_json(device_key)}};

  const auto data = exchange(m_endpoint, m_trust, "/buy", body);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  return receipt_of(data.value());
}

Result<PurchaseSummaries> HttpTransport::purchases(const Credentials &credentials, std::size_t offset,
                                                   std::size_t limit) {
  const Json body{{"credentials", credentials_json(credentials)},
                  {"offset", std::to_string(offset)},
                  {"limit", std::to_string(limit)}};
  const auto data = exchange(m_endpoint, m_trust, "/purchases", body);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  if (!data->is_array()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  std::vector<core::PurchaseSummary> summaries;

  for (const Json &entry : data.value()) {
    const auto summary = purchase_summary_of(entry);

    if (!summary.has_value()) {
      return std::unexpected(summary.error());
    }

    summaries.push_back(summary.value());
  }

  return summaries;
}

Result<Receipt> HttpTransport::restore_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                               const DevicePublicKey &device_key) {
  const Json body{{"credentials", credentials_json(credentials)},
                  {"purchase_id", std::to_string(purchase_id)},
                  {"device_key", hex_json(device_key)}};

  const auto data = exchange(m_endpoint, m_trust, "/restore-receipt", body);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  return receipt_of(data.value());
}

Result<ContentIdentity> HttpTransport::context_identity(const Credentials &credentials, const PurchaseId &context_id) {
  const Json body{{"credentials", credentials_json(credentials)}, {"context_id", std::to_string(context_id)}};

  const auto data = exchange(m_endpoint, m_trust, "/context-identity", body);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  if (!data->is_string()) {
    return std::unexpected(core::CoreError::protocol_failure);
  }

  return identity_of_text(data->get<std::string>());
}

Result<Package> HttpTransport::fetch_package(const Credentials &credentials, const PurchaseId &purchase_id,
                                             const DevicePublicKey &device_key) {
  const Json body{{"credentials", credentials_json(credentials)},
                  {"purchase_id", std::to_string(purchase_id)},
                  {"device_key", hex_json(device_key)}};

  const auto data = exchange(m_endpoint, m_trust, "/fetch-package", body);

  if (!data.has_value()) {
    return std::unexpected(data.error());
  }

  return package_of(data.value());
}

} // namespace dgds::client
