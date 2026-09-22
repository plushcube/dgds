#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/user.h>

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace dgds::server {

using core::Credentials;
using core::Result;
using core::UserId;

class SessionStore {
public:
  [[nodiscard]] Result<Credentials> issue(const UserId &user_id);
  [[nodiscard]] Result<UserId> resolve(core::Content token) const;

private:
  struct TokenHash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(core::Content token) const { return std::hash<core::Content>{}(token); }
    [[nodiscard]] std::size_t operator()(const std::string &token) const { return std::hash<core::Content>{}(token); }
  };

  using Sessions = std::unordered_map<std::string, UserId, TokenHash, std::equal_to<>>;

  mutable std::mutex m_mutex;
  Sessions m_sessions;
};

} // namespace dgds::server
