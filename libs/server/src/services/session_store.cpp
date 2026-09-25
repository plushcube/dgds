#include <dgds/server/services/session_store.h>

#include <dgds/core/identity/content_identity.h>

#include <openssl/rand.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace dgds::server {
namespace {

constexpr std::size_t k_token_size = 32;

} // namespace

Result<Credentials> SessionStore::issue(const UserId &user_id) {
  std::array<std::uint8_t, k_token_size> bytes{};

  if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  Credentials credentials{.user_id = user_id, .token = core::to_hex(bytes.data(), bytes.size())};
  const Clock::time_point now = Clock::now();

  const std::lock_guard lock(m_mutex);
  prune(now);
  drop_sessions_of(user_id);
  m_sessions.insert_or_assign(credentials.token, Session{.user_id = user_id, .expires_at = now + m_lifetime});

  return credentials;
}

Result<UserId> SessionStore::resolve(core::Content token) {
  const Clock::time_point now = Clock::now();

  const std::lock_guard lock(m_mutex);
  prune(now);

  const auto found = m_sessions.find(token);

  if (found == m_sessions.end()) {
    return std::unexpected(core::CoreError::authorization_failed);
  }

  return found->second.user_id;
}

void SessionStore::prune(Clock::time_point now) {
  for (auto entry = m_sessions.begin(); entry != m_sessions.end();) {
    if (entry->second.expires_at <= now) {
      entry = m_sessions.erase(entry);
      continue;
    }

    ++entry;
  }
}

void SessionStore::drop_sessions_of(const UserId &user_id) {
  for (auto entry = m_sessions.begin(); entry != m_sessions.end();) {
    if (entry->second.user_id == user_id) {
      entry = m_sessions.erase(entry);
      continue;
    }

    ++entry;
  }
}

} // namespace dgds::server
