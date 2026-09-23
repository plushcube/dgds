#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/ports/metadata_registry.h>
#include <dgds/server/models/attribution.h>

namespace dgds::server {

class AttributionService {
public:
  explicit AttributionService(core::MetadataRegistry &metadata) : m_metadata(metadata) {}

  [[nodiscard]] core::Result<Attribution> attribute(core::Content leaked_text);

private:
  core::MetadataRegistry &m_metadata;
};

} // namespace dgds::server
