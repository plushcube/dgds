#include <dgds/server/api/codec.h>

#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/user_id.h>

#include <nlohmann/json.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <system_error>

namespace dgds::server::api {
namespace {

using Json = nlohmann::json;

constexpr const char *k_algorithm = "algorithm";
constexpr const char *k_nonce = "nonce";
constexpr const char *k_ciphertext = "ciphertext";
constexpr const char *k_tag = "tag";
constexpr const char *k_ephemeral_key = "ephemeral_key";
constexpr const char *k_wrapped = "wrapped";

template <std::size_t Size> std::string hex_of(const std::array<std::uint8_t, Size> &bytes) {
  return core::to_hex(bytes.data(), bytes.size());
}

template <std::size_t Size> std::optional<std::array<std::uint8_t, Size>> bytes_of(core::Content text) {
  const auto decoded = core::from_hex(text);

  if (!decoded.has_value() || decoded->size() != Size) {
    return std::nullopt;
  }

  std::array<std::uint8_t, Size> bytes{};
  std::memcpy(bytes.data(), decoded->data(), Size);

  return bytes;
}

std::optional<Json> parse(core::Content body) {
  const Json value = Json::parse(body, nullptr, false);

  if (value.is_discarded() || !value.is_object()) {
    return std::nullopt;
  }

  return value;
}

std::optional<std::string> read_string(const Json &body, core::Content field) {
  const auto found = body.find(std::string(field));

  if (found == body.end() || !found->is_string()) {
    return std::nullopt;
  }

  return found->get<std::string>();
}

std::optional<std::uint64_t> read_number(const Json &body, core::Content field) {
  const auto text = read_string(body, field);

  if (!text.has_value()) {
    return std::nullopt;
  }

  std::uint64_t value = 0;
  const char *begin = text->data();
  const char *end = begin + text->size();
  const std::from_chars_result parsed = std::from_chars(begin, end, value);

  if (parsed.ec != std::errc() || parsed.ptr != end) {
    return std::nullopt;
  }

  return value;
}

Json sealed_json(const core::SealedContent &value) {
  return Json{{k_algorithm, static_cast<std::uint8_t>(value.algorithm)},
              {k_nonce, hex_of(value.nonce)},
              {k_ciphertext,
               core::to_hex(reinterpret_cast<const std::uint8_t *>(value.ciphertext.data()), value.ciphertext.size())},
              {k_tag, hex_of(value.tag)}};
}

Json envelope_json(const core::DeviceEnvelope &value) {
  return Json{{k_ephemeral_key, hex_of(value.ephemeral_key)}, {k_wrapped, sealed_json(value.wrapped)}};
}

Json summary_json(const core::PublicationSummary &value) {
  return Json{{"publication_id", std::to_string(value.publication_id)},
              {"title", value.title},
              {"file_name", value.file_name},
              {"size", value.size},
              {"published_at", value.published_at},
              {"author_name", value.author_name}};
}

Json purchase_summary_json(const core::PurchaseSummary &value) {
  return Json{{"purchase_id", std::to_string(value.purchase_id)},
              {"publication", summary_json(value.publication)},
              {"purchased_at", value.purchased_at}};
}

Json receipt_json(const core::Receipt &value) {
  return Json{{"header", Json{{"version", value.header.version},
                              {"purchase_id", std::to_string(value.header.purchase_id)},
                              {"user_id", core::to_uuid(value.header.user_id)},
                              {"purchased_at", value.header.purchased_at},
                              {"issued_at", value.header.issued_at}}},
              {"wrapped_key", envelope_json(value.wrapped_key)}};
}

Json package_json(const core::Package &value) {
  return Json{{"version", value.version},
              {"content", sealed_json(value.content)},
              {"wrapped_blob_key", sealed_json(value.wrapped_blob_key)},
              {"identity", hex_of(value.identity)},
              {"author_key", hex_of(value.author_key)},
              {"author_name", value.author_name},
              {"signature_algorithm", static_cast<std::uint8_t>(value.signature_algorithm)},
              {"signature", hex_of(value.signature)}};
}

} // namespace

std::optional<core::Credentials> read_credentials(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const auto found = parsed->find("credentials");

  if (found == parsed->end() || !found->is_object()) {
    return std::nullopt;
  }

  const auto user_id = read_string(found.value(), "user_id");
  const auto token = read_string(found.value(), "token");

  if (!user_id.has_value() || !token.has_value()) {
    return std::nullopt;
  }

  const auto bytes = core::from_uuid(user_id.value());

  if (!bytes.has_value()) {
    return std::nullopt;
  }

  return core::Credentials{.user_id = bytes.value(), .token = token.value()};
}

std::optional<core::ContentBuffer> read_name(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const auto name = read_string(parsed.value(), "name");

  if (!name.has_value()) {
    return std::nullopt;
  }

  return name.value();
}

std::optional<core::PublicationDraft> read_draft(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const auto draft = parsed->find("draft");

  if (draft == parsed->end() || !draft->is_object()) {
    return std::nullopt;
  }

  const auto title = read_string(draft.value(), "title");
  const auto file_name = read_string(draft.value(), "file_name");
  const auto content = read_string(draft.value(), "content");

  if (!title.has_value() || !file_name.has_value() || !content.has_value()) {
    return std::nullopt;
  }

  return core::PublicationDraft{.title = title.value(), .file_name = file_name.value(), .content = content.value()};
}

std::optional<core::PublicationId> read_publication_id(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  return read_number(parsed.value(), "publication_id");
}

std::optional<core::PurchaseId> read_purchase_id(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  return read_number(parsed.value(), "purchase_id");
}

std::optional<core::PurchaseId> read_context_id(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  return read_number(parsed.value(), "context_id");
}

std::optional<core::DevicePublicKey> read_device_key(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const auto text = read_string(parsed.value(), "device_key");

  if (!text.has_value()) {
    return std::nullopt;
  }

  return bytes_of<core::k_device_key_size>(text.value());
}

std::optional<core::AuthorPublicKey> read_author_key(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const auto text = read_string(parsed.value(), "author_key");

  if (!text.has_value()) {
    return std::nullopt;
  }

  return bytes_of<core::k_author_public_key_size>(text.value());
}

std::optional<core::Signature> read_signature(core::Content body) {
  const auto parsed = parse(body);

  if (!parsed.has_value()) {
    return std::nullopt;
  }

  const auto text = read_string(parsed.value(), "signature");

  if (!text.has_value()) {
    return std::nullopt;
  }

  return bytes_of<core::k_signature_size>(text.value());
}

std::string encode(const core::UserAccount &account) {
  return Json{{"user_id", core::to_uuid(account.user_id)}, {"name", account.name}}.dump();
}

std::string encode(const core::Credentials &credentials) {
  return Json{{"user_id", core::to_uuid(credentials.user_id)}, {"token", credentials.token}}.dump();
}

std::string encode(const core::PublicationSummaries &summaries) {
  Json list = Json::array();

  for (const auto &summary : summaries) {
    list.push_back(summary_json(summary));
  }

  return list.dump();
}

std::string encode(const core::AuthorPublicationSummaries &summaries) {
  Json list = Json::array();

  for (const auto &summary : summaries) {
    list.push_back(Json{{"publication", summary_json(summary.publication)}, {"purchases", summary.purchases}});
  }

  return list.dump();
}

std::string encode(const core::PurchaseSummaries &summaries) {
  Json list = Json::array();

  for (const auto &summary : summaries) {
    list.push_back(purchase_summary_json(summary));
  }

  return list.dump();
}

std::string encode(const core::Receipt &receipt) { return receipt_json(receipt).dump(); }

std::string encode(const core::Package &package) { return package_json(package).dump(); }

std::string encode(const core::ContentIdentity &identity) { return Json(hex_of(identity)).dump(); }

std::string encode_id(core::PurchaseId id) { return Json(std::to_string(id)).dump(); }

} // namespace dgds::server::api
