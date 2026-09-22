#pragma once

#include <dgds/core/models/publication.h>

namespace dgds::server {

inline core::PublicationSummary summarise_publication(const core::PublicationRecord &record) {
  return core::PublicationSummary{.publication_id = record.publication_id,
                                  .title = record.title,
                                  .file_name = record.file_name,
                                  .size = record.size,
                                  .published_at = record.published_at,
                                  .author_name = record.author_name};
}

} // namespace dgds::server
