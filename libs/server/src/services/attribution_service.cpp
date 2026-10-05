#include <dgds/server/services/attribution_service.h>

#include <dgds/core/watermark/mark_channel.h>

#include "access.h"

#include <expected>
#include <optional>

namespace dgds::server {

core::Result<Attribution> AttributionService::attribute(core::Content leaked_text) {
  const auto marks = core::read_marks(leaked_text);

  if (!marks.has_value()) {
    return std::unexpected(marks.error());
  }

  std::optional<Access> matched;
  core::CoreError resolution_error = core::CoreError::purchase_not_found;
  bool resolved_any = false;

  for (const core::Mark &mark : marks.value()) {
    const auto access = resolve_context(mark.purchase_id, m_metadata);

    if (!access.has_value()) {
      resolution_error = access.error();
      continue;
    }

    resolved_any = true;

    const auto authentic = m_keys.verify_mark(access->publication.identity, mark);

    if (!authentic.has_value()) {
      return std::unexpected(authentic.error());
    }

    if (!authentic.value()) {
      continue;
    }

    if (matched.has_value()) {
      return std::unexpected(core::CoreError::mark_not_confident);
    }

    matched = access.value();
  }

  if (!matched.has_value()) {
    return std::unexpected(resolved_any ? core::CoreError::mark_authentication_failed : resolution_error);
  }

  const auto user = m_metadata.find_user(matched->user_id);

  if (!user.has_value()) {
    return std::unexpected(user.error());
  }

  return Attribution{.kind = matched->kind,
                     .context_id = matched->context_id,
                     .user_id = user->user_id,
                     .user_name = user->name,
                     .publication_id = matched->publication.publication_id,
                     .title = matched->publication.title,
                     .granted_at = matched->granted_at};
}

} // namespace dgds::server
