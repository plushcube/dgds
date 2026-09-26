#include <dgds/server/services/catalog_service.h>

#include "access.h"
#include "summaries.h"

#include <expected>

namespace dgds::server {

core::Result<core::PublicationSummaryPage> CatalogService::catalog(core::PageRequest request) {
  const auto page = m_metadata.publications(request.offset, request.limit);

  if (!page.has_value()) {
    return std::unexpected(page.error());
  }

  core::PublicationSummaryPage summaries{.total = page->total, .records = {}};
  summaries.records.reserve(page->records.size());

  for (const auto &record : page->records) {
    summaries.records.push_back(summarise_publication(record));
  }

  return summaries;
}

core::Result<core::AuthorPublicationSummaryPage> CatalogService::author_publications(const core::UserId &author_id,
                                                                                     core::PageRequest request) {
  const auto page = m_metadata.publications_of_author(author_id, request.offset, request.limit);

  if (!page.has_value()) {
    return std::unexpected(page.error());
  }

  core::AuthorPublicationSummaryPage summaries{.total = page->total, .records = {}};
  summaries.records.reserve(page->records.size());

  for (const auto &record : page->records) {
    const auto purchases = m_metadata.purchase_count(record.publication_id);

    if (!purchases.has_value()) {
      return std::unexpected(purchases.error());
    }

    summaries.records.push_back(
        core::AuthorPublicationSummary{.publication = summarise_publication(record), .purchases = purchases.value()});
  }

  return summaries;
}

core::Result<core::ContentIdentity> CatalogService::context_identity(const core::UserId &user_id,
                                                                     const core::PurchaseId &context_id) {
  const auto access = resolve_access(user_id, context_id, m_metadata);

  if (!access.has_value()) {
    return std::unexpected(access.error());
  }

  return access->publication.identity;
}

} // namespace dgds::server
