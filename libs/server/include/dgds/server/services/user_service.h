#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/user.h>
#include <dgds/core/ports/metadata_registry.h>

namespace dgds::server {

class SessionStore;

class UserService {
public:
  UserService(core::MetadataRegistry &metadata, SessionStore &sessions) : m_metadata(metadata), m_sessions(sessions) {}

  [[nodiscard]] core::Result<core::UserAccount> register_user(core::Content name);
  [[nodiscard]] core::Result<core::Credentials> log_in(core::Content name);

private:
  core::MetadataRegistry &m_metadata;
  SessionStore &m_sessions;
};

} // namespace dgds::server
