#pragma once

#include <dgds/core/models/publication.h>
#include <dgds/core/models/user.h>
#include <dgds/core/ports/metadata_registry.h>

namespace dgds::server {

class CatalogService {
public:
  explicit CatalogService(core::MetadataRegistry &metadata) : m_metadata(metadata) {}

  [[nodiscard]] core::Result<core::PublicationSummaries> catalog();
  [[nodiscard]] core::Result<core::AuthorPublicationSummaries> author_publications(const core::UserId &author_id);

private:
  core::MetadataRegistry &m_metadata;
};

} // namespace dgds::server
