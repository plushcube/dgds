#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/user.h>

#include <chrono>
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
  inline static constexpr std::chrono::seconds k_default_lifetime{12 * 60 * 60};

  explicit SessionStore(std::chrono::seconds lifetime = k_default_lifetime) : m_lifetime(lifetime) {}

  [[nodiscard]] Result<Credentials> issue(const UserId &user_id);
  [[nodiscard]] Result<UserId> resolve(core::Content token);

private:
  using Clock = std::chrono::steady_clock;

  struct TokenHash {
    using is_transparent = void;

    [[nodiscard]] std::size_t operator()(core::Content token) const { return std::hash<core::Content>{}(token); }
    [[nodiscard]] std::size_t operator()(const std::string &token) const { return std::hash<core::Content>{}(token); }
  };

  struct Session {
    UserId user_id;
    Clock::time_point expires_at;
  };

  using Sessions = std::unordered_map<std::string, Session, TokenHash, std::equal_to<>>;

  void prune(Clock::time_point now);
  void drop_sessions_of(const UserId &user_id);

  std::chrono::seconds m_lifetime;
  std::mutex m_mutex;
  Sessions m_sessions;
};

} // namespace dgds::server
