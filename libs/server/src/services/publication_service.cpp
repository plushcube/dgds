#include <dgds/server/services/publication_service.h>

#include <dgds/core/envelope/keys.h>
#include <dgds/core/identity/content_identity.h>
#include <dgds/core/identity/identifier.h>
#include <dgds/core/signature/author_signature.h>

#include <cstddef>
#include <cstring>
#include <expected>

namespace dgds::server {
namespace {

core::CoreError release_claim(core::IdentityRegistry &identities, const core::ContentIdentity &identity,
                              core::CoreError original) {
  const auto released = identities.release(identity);

  return released.has_value() ? original : released.error();
}

} // namespace

core::Result<core::PublicationRecord> PublicationService::publish(const core::UserId &author_id,
                                                                  const core::PublicationDraft &draft,
                                                                  const core::AuthorPublicKey &author_key,
                                                                  const core::Signature &signature,
                                                                  core::Timestamp published_at) {
  const auto author = m_metadata.find_user(author_id);

  if (!author.has_value()) {
    return std::unexpected(author.error());
  }

  const core::CanonicalForm canonical = core::canonical_form(draft.content);
  const auto identity = core::content_identity(canonical);

  if (!identity.has_value()) {
    return std::unexpected(identity.error());
  }

  const auto verified = core::verify_author(identity.value(), author->name, signature, author_key);

  if (!verified.has_value()) {
    return std::unexpected(verified.error());
  }

  if (!verified.value()) {
    return std::unexpected(core::CoreError::signature_invalid);
  }

  const auto claimed = m_identities.claim(identity.value());

  if (!claimed.has_value()) {
    return std::unexpected(claimed.error());
  }

  if (claimed.value() == core::ClaimOutcome::already_claimed) {
    return std::unexpected(core::CoreError::content_duplicate);
  }

  core::SecureBuffer plaintext(canonical.size());

  if (!canonical.empty()) {
    std::memcpy(plaintext.data(), canonical.data(), canonical.size());
  }

  const auto sealed = m_keys.seal(identity.value(), plaintext, core::as_content(identity.value()));

  if (!sealed.has_value()) {
    return std::unexpected(release_claim(m_identities, identity.value(), sealed.error()));
  }

  const auto stored = m_blobs.store(identity.value(), sealed.value());

  if (!stored.has_value()) {
    return std::unexpected(release_claim(m_identities, identity.value(), stored.error()));
  }

  const auto publication_id = core::generate_identifier();

  if (!publication_id.has_value()) {
    return std::unexpected(release_claim(m_identities, identity.value(), publication_id.error()));
  }

  const core::PublicationRecord publication{.publication_id = publication_id.value(),
                                            .author_id = author_id,
                                            .author_name = author->name,
                                            .title = draft.title,
                                            .file_name = draft.file_name,
                                            .size = canonical.size(),
                                            .published_at = published_at,
                                            .identity = identity.value(),
                                            .author_key = author_key,
                                            .signature_algorithm = core::k_signature_algorithm,
                                            .signature = signature};

  const auto registered = m_metadata.add_publication(publication);

  if (!registered.has_value()) {
    return std::unexpected(release_claim(m_identities, identity.value(), registered.error()));
  }

  return publication;
}

} // namespace dgds::server
