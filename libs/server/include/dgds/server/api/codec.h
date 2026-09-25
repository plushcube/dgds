#pragma once

#include <dgds/core/models/author.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/device.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/identity.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>

#include <cstdint>
#include <optional>
#include <string>

namespace dgds::server::api {

[[nodiscard]] std::optional<std::uint8_t> read_version(core::Content body);
[[nodiscard]] std::optional<core::Credentials> read_credentials(core::Content body);
[[nodiscard]] std::optional<core::ContentBuffer> read_name(core::Content body);
[[nodiscard]] std::optional<core::PublicationDraft> read_draft(core::Content body);
[[nodiscard]] std::optional<core::PublicationId> read_publication_id(core::Content body);
[[nodiscard]] std::optional<core::PurchaseId> read_purchase_id(core::Content body);
[[nodiscard]] std::optional<core::PurchaseId> read_context_id(core::Content body);
[[nodiscard]] std::optional<core::DevicePublicKey> read_device_key(core::Content body);
[[nodiscard]] std::optional<core::AuthorPublicKey> read_author_key(core::Content body);
[[nodiscard]] std::optional<core::Signature> read_signature(core::Content body);

[[nodiscard]] std::string encode(const core::UserAccount &account);
[[nodiscard]] std::string encode(const core::Credentials &credentials);
[[nodiscard]] std::string encode(const core::PublicationSummaries &summaries);
[[nodiscard]] std::string encode(const core::AuthorPublicationSummaries &summaries);
[[nodiscard]] std::string encode(const core::PurchaseSummaries &summaries);
[[nodiscard]] std::string encode(const core::Receipt &receipt);
[[nodiscard]] std::string encode(const core::Package &package);
[[nodiscard]] std::string encode(const core::ContentIdentity &identity);
[[nodiscard]] std::string encode_id(core::PurchaseId id);

} // namespace dgds::server::api
