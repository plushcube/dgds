#pragma once

#include <dgds/core/models/author.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/identity.h>
#include <dgds/core/models/page.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dgds::core {

using PublicationId = std::uint64_t;

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

struct PublicationSummary {
  PublicationId publication_id;
  ContentBuffer title;
  ContentBuffer file_name;
  std::size_t size;
  Timestamp published_at;
  ContentBuffer author_name;
};

struct PublicationDraft {
  ContentBuffer title;
  ContentBuffer file_name;
  ContentBuffer content;
};

struct AuthorPublicationSummary {
  PublicationSummary publication;
  std::size_t purchases;
};

using PublicationSummaries = std::vector<PublicationSummary>;
using AuthorPublicationSummaries = std::vector<AuthorPublicationSummary>;

using PublicationPage = Page<PublicationRecord>;
using PublicationSummaryPage = Page<PublicationSummary>;
using AuthorPublicationSummaryPage = Page<AuthorPublicationSummary>;

} // namespace dgds::core
