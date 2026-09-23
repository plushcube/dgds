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

  std::string token = core::to_hex(bytes.data(), bytes.size());

  const std::lock_guard lock(m_mutex);
  m_sessions.insert_or_assign(token, user_id);

  return Credentials{.user_id = user_id, .token = std::move(token)};
}

Result<UserId> SessionStore::resolve(core::Content token) const {
  const std::lock_guard lock(m_mutex);

  const auto found = m_sessions.find(token);

  if (found == m_sessions.end()) {
    return std::unexpected(core::CoreError::authorization_failed);
  }

  return found->second;
}

} // namespace dgds::server
