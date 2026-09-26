#include <dgds/server/middleware/rate_limiter.h>

#include <dgds/core/identity/content_identity.h>

#include <mutex>
#include <string>
#include <string_view>

namespace dgds::server {

bool RateLimiter::accepted(LimitedOperation operation, const core::UserId &user_id, core::Timestamp now) {
  return accepted(operation, core::to_hex(user_id.data(), user_id.size()), now);
}

bool RateLimiter::accepted(LimitedOperation operation, std::string_view key, core::Timestamp now) {
  if (m_limit.calls == 0) {
    return true;
  }

  const std::scoped_lock guard(m_mutex);

  if (m_counters.size() >= k_prune_threshold && ++m_calls_since_prune >= k_prune_interval) {
    m_calls_since_prune = 0;
    prune(now);
  }

  Counter &counter = m_counters[Key{operation, std::string(key)}];

  if (now < counter.started_at || now >= counter.started_at + m_limit.window) {
    counter.started_at = now;
    counter.calls = 0;
  }

  if (counter.calls >= m_limit.calls) {
    return false;
  }

  ++counter.calls;

  return true;
}

void RateLimiter::prune(core::Timestamp now) {
  for (auto entry = m_counters.begin(); entry != m_counters.end();) {
    if (now >= entry->second.started_at && now >= entry->second.started_at + m_limit.window) {
      entry = m_counters.erase(entry);
      continue;
    }

    ++entry;
  }
}

} // namespace dgds::server
