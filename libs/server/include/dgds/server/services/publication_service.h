#pragma once

#include <dgds/core/models/author.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/user.h>
#include <dgds/core/ports/blob_store.h>
#include <dgds/core/ports/identity_registry.h>
#include <dgds/core/ports/key_store.h>
#include <dgds/core/ports/metadata_registry.h>

namespace dgds::server {

class PublicationService {
public:
  PublicationService(core::IdentityRegistry &identities, core::KeyStore &keys, core::BlobStore &blobs,
                     core::MetadataRegistry &metadata)
      : m_identities(identities), m_keys(keys), m_blobs(blobs), m_metadata(metadata) {}

  [[nodiscard]] core::Result<core::PublicationRecord>
  publish(const core::UserId &author_id, const core::PublicationDraft &draft, const core::AuthorPublicKey &author_key,
          const core::Signature &signature, core::Timestamp published_at);

private:
  core::IdentityRegistry &m_identities;
  core::KeyStore &m_keys;
  core::BlobStore &m_blobs;
  core::MetadataRegistry &m_metadata;
};

} // namespace dgds::server
