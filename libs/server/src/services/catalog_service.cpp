#include <dgds/server/services/catalog_service.h>

#include <expected>

namespace dgds::server {
namespace {

core::PublicationSummary summarise(const core::PublicationRecord &record) {
  return core::PublicationSummary{.publication_id = record.publication_id,
                                  .title = record.title,
                                  .file_name = record.file_name,
                                  .size = record.size,
                                  .published_at = record.published_at,
                                  .author_name = record.author_name};
}

} // namespace

core::Result<core::PublicationSummaries> CatalogService::catalog() {
  const auto records = m_metadata.publications();

  if (!records.has_value()) {
    return std::unexpected(records.error());
  }

  core::PublicationSummaries summaries;
  summaries.reserve(records->size());

  for (const auto &record : records.value()) {
    summaries.push_back(summarise(record));
  }

  return summaries;
}

core::Result<core::AuthorPublicationSummaries> CatalogService::author_publications(const core::UserId &author_id) {
  const auto records = m_metadata.publications_of_author(author_id);

  if (!records.has_value()) {
    return std::unexpected(records.error());
  }

  core::AuthorPublicationSummaries summaries;
  summaries.reserve(records->size());

  for (const auto &record : records.value()) {
    const auto purchases = m_metadata.purchase_count(record.publication_id);

    if (!purchases.has_value()) {
      return std::unexpected(purchases.error());
    }

    summaries.push_back(
        core::AuthorPublicationSummary{.publication = summarise(record), .purchases = purchases.value()});
  }

  return summaries;
}

} // namespace dgds::server
