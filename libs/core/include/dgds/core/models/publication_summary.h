#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/publication_id.h>
#include <dgds/core/models/timestamp.h>

#include <cstddef>

namespace dgds::core {

struct PublicationSummary {
  PublicationId publication_id;
  ContentBuffer title;
  ContentBuffer file_name;
  std::size_t size;
  Timestamp published_at;
  ContentBuffer author_name;
};

} // namespace dgds::core
