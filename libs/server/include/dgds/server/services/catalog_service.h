#pragma once

#include <dgds/core/models/identity.h>
#include <dgds/core/models/publication.h>
#include <dgds/core/models/purchase.h>
#include <dgds/core/models/user.h>
#include <dgds/core/ports/metadata_registry.h>

namespace dgds::server {

class CatalogService {
public:
  explicit CatalogService(core::MetadataRegistry &metadata) : m_metadata(metadata) {}

  [[nodiscard]] core::Result<core::PublicationSummaryPage> catalog(core::PageRequest request);
  [[nodiscard]] core::Result<core::AuthorPublicationSummaryPage> author_publications(const core::UserId &author_id,
                                                                                     core::PageRequest request);

  [[nodiscard]] core::Result<core::ContentIdentity> context_identity(const core::UserId &user_id,
                                                                     const core::PurchaseId &context_id);

private:
  core::MetadataRegistry &m_metadata;
};

} // namespace dgds::server
