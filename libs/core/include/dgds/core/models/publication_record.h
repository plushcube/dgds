#pragma once

#include <dgds/core/models/author_key.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/content_identity.h>
#include <dgds/core/models/publication_id.h>
#include <dgds/core/models/signature.h>
#include <dgds/core/models/signature_algorithm.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user_id.h>

#include <cstddef>

namespace dgds::core {

struct PublicationRecord {
  PublicationId publication_id;
  UserId author_id;
  ContentBuffer author_name;
  ContentBuffer title;
  ContentBuffer file_name;
  std::size_t size;
  Timestamp published_at;
  ContentIdentity identity;
  AuthorPublicKey author_key;
  SignatureAlgorithm signature_algorithm;
  Signature signature;
};

} // namespace dgds::core
