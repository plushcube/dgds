#include <dgds/server/api/surface.h>

#include <dgds/core/models/protocol.h>
#include <dgds/server/api/codec.h>
#include <dgds/server/api/response.h>

#include <expected>
#include <optional>
#include <string>

namespace dgds::server::api {
namespace {

constexpr ProtocolError k_malformed = ProtocolError::request_malformed;

} // namespace

std::optional<ProtocolError> Surface::version_failure(core::Content body) {
  const auto version = read_version(body);

  if (!version.has_value()) {
    return k_malformed;
  }

  if (version.value() != core::k_protocol_version) {
    return ProtocolError::version_unsupported;
  }

  return std::nullopt;
}

core::Result<core::UserId> Surface::authorized(const core::Credentials &credentials) {
  const auto user_id = m_sessions.resolve(credentials.token);

  if (!user_id.has_value()) {
    return std::unexpected(user_id.error());
  }

  if (user_id.value() != credentials.user_id) {
    return std::unexpected(core::CoreError::authorization_failed);
  }

  return user_id.value();
}

std::string Surface::unknown_operation() { return failure(ProtocolError::operation_unknown); }

std::string Surface::register_user(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto name = read_name(body);

  if (!name.has_value()) {
    return failure(k_malformed);
  }

  const auto account = m_users.register_user(name.value());

  if (!account.has_value()) {
    return failure(account.error());
  }

  return ok(encode(account.value()));
}

std::string Surface::log_in(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto name = read_name(body);

  if (!name.has_value()) {
    return failure(k_malformed);
  }

  const auto credentials = m_users.log_in(name.value());

  if (!credentials.has_value()) {
    return failure(credentials.error());
  }

  return ok(encode(credentials.value()));
}

std::string Surface::catalog(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto summaries = m_catalog.catalog();

  if (!summaries.has_value()) {
    return failure(summaries.error());
  }

  return ok(encode(summaries.value()));
}

std::string Surface::publish(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto credentials = read_credentials(body);
  const auto draft = read_draft(body);
  const auto author_key = read_author_key(body);
  const auto signature = read_signature(body);

  if (!credentials.has_value() || !draft.has_value() || !author_key.has_value() || !signature.has_value()) {
    return failure(k_malformed);
  }

  const auto author_id = authorized(credentials.value());

  if (!author_id.has_value()) {
    return failure(author_id.error());
  }

  const auto publication =
      m_publications.publish(author_id.value(), draft.value(), author_key.value(), signature.value(), m_clock());

  if (!publication.has_value()) {
    return failure(publication.error());
  }

  return ok(encode_id(publication->publication_id));
}

std::string Surface::author_publications(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto credentials = read_credentials(body);

  if (!credentials.has_value()) {
    return failure(k_malformed);
  }

  const auto author_id = authorized(credentials.value());

  if (!author_id.has_value()) {
    return failure(author_id.error());
  }

  const auto summaries = m_catalog.author_publications(author_id.value());

  if (!summaries.has_value()) {
    return failure(summaries.error());
  }

  return ok(encode(summaries.value()));
}

std::string Surface::buy(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto credentials = read_credentials(body);
  const auto publication_id = read_publication_id(body);
  const auto device_key = read_device_key(body);

  if (!credentials.has_value() || !publication_id.has_value() || !device_key.has_value()) {
    return failure(k_malformed);
  }

  const auto buyer_id = authorized(credentials.value());

  if (!buyer_id.has_value()) {
    return failure(buyer_id.error());
  }

  const auto receipt = m_purchases.buy(buyer_id.value(), publication_id.value(), device_key.value(), m_clock());

  if (!receipt.has_value()) {
    return failure(receipt.error());
  }

  return ok(encode(receipt.value()));
}

std::string Surface::purchases(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto credentials = read_credentials(body);

  if (!credentials.has_value()) {
    return failure(k_malformed);
  }

  const auto user_id = authorized(credentials.value());

  if (!user_id.has_value()) {
    return failure(user_id.error());
  }

  const auto summaries = m_purchases.purchases_of(user_id.value());

  if (!summaries.has_value()) {
    return failure(summaries.error());
  }

  return ok(encode(summaries.value()));
}

std::string Surface::restore_receipt(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto credentials = read_credentials(body);
  const auto purchase_id = read_purchase_id(body);
  const auto device_key = read_device_key(body);

  if (!credentials.has_value() || !purchase_id.has_value() || !device_key.has_value()) {
    return failure(k_malformed);
  }

  const auto user_id = authorized(credentials.value());

  if (!user_id.has_value()) {
    return failure(user_id.error());
  }

  const auto receipt = m_purchases.restore_receipt(user_id.value(), purchase_id.value(), device_key.value(), m_clock());

  if (!receipt.has_value()) {
    return failure(receipt.error());
  }

  return ok(encode(receipt.value()));
}

std::string Surface::context_identity(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto credentials = read_credentials(body);
  const auto context_id = read_context_id(body);

  if (!credentials.has_value() || !context_id.has_value()) {
    return failure(k_malformed);
  }

  const auto user_id = authorized(credentials.value());

  if (!user_id.has_value()) {
    return failure(user_id.error());
  }

  const auto identity = m_catalog.context_identity(user_id.value(), context_id.value());

  if (!identity.has_value()) {
    return failure(identity.error());
  }

  return ok(encode(identity.value()));
}

std::string Surface::fetch_package(core::Content body) {
  if (const auto failure_code = version_failure(body); failure_code.has_value()) {
    return failure(failure_code.value());
  }

  const auto credentials = read_credentials(body);
  const auto purchase_id = read_purchase_id(body);
  const auto device_key = read_device_key(body);

  if (!credentials.has_value() || !purchase_id.has_value() || !device_key.has_value()) {
    return failure(k_malformed);
  }

  const auto user_id = authorized(credentials.value());

  if (!user_id.has_value()) {
    return failure(user_id.error());
  }

  const auto package = m_delivery.fetch_package(user_id.value(), purchase_id.value(), device_key.value());

  if (!package.has_value()) {
    return failure(package.error());
  }

  return ok(encode(package.value()));
}

} // namespace dgds::server::api
