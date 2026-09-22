#include <dgds/server/services/user_service.h>

#include <dgds/core/identity/user_id.h>
#include <dgds/server/services/session_store.h>

#include <expected>

namespace dgds::server {

core::Result<core::UserAccount> UserService::register_user(core::Content name) {
  const auto user_id = core::generate_user_id();

  if (!user_id.has_value()) {
    return std::unexpected(user_id.error());
  }

  const core::UserAccount account{.user_id = user_id.value(), .name = core::ContentBuffer(name)};

  const auto added = m_metadata.add_user(account);

  if (!added.has_value()) {
    return std::unexpected(added.error());
  }

  return account;
}

core::Result<core::Credentials> UserService::log_in(core::Content name) {
  const auto account = m_metadata.find_user_by_name(name);

  if (!account.has_value()) {
    return std::unexpected(account.error());
  }

  return m_sessions.issue(account->user_id);
}

} // namespace dgds::server
