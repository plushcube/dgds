#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/ports/key_store.h>
#include <dgds/core/ports/metadata_registry.h>
#include <dgds/server/models/attribution.h>

namespace dgds::server {

class AttributionService {
public:
  AttributionService(core::MetadataRegistry &metadata, core::KeyStore &keys) : m_metadata(metadata), m_keys(keys) {}

  [[nodiscard]] core::Result<Attribution> attribute(core::Content leaked_text);

private:
  core::MetadataRegistry &m_metadata;
  core::KeyStore &m_keys;
};

} // namespace dgds::server
