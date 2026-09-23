#include <dgds/server/services/attribution_service.h>

#include <dgds/core/watermark/mark_channel.h>

#include "access.h"

#include <expected>

namespace dgds::server {

core::Result<Attribution> AttributionService::attribute(core::Content leaked_text) {
  const auto mark = core::read_mark(leaked_text);

  if (!mark.has_value()) {
    return std::unexpected(mark.error());
  }

  const auto access = resolve_context(mark->purchase_id, m_metadata);

  if (!access.has_value()) {
    return std::unexpected(access.error());
  }

  const auto user = m_metadata.find_user(access->user_id);

  if (!user.has_value()) {
    return std::unexpected(user.error());
  }

  return Attribution{.kind = access->kind,
                     .context_id = access->context_id,
                     .user_id = user->user_id,
                     .user_name = user->name,
                     .publication_id = access->publication.publication_id,
                     .title = access->publication.title,
                     .granted_at = access->granted_at};
}

} // namespace dgds::server
