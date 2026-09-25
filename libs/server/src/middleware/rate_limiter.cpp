#include <dgds/server/middleware/rate_limiter.h>

#include <mutex>

namespace dgds::server {

bool RateLimiter::accepted(LimitedOperation operation, const core::UserId &user_id, core::Timestamp now) {
  if (m_limit.calls == 0) {
    return true;
  }

  const std::scoped_lock guard(m_mutex);

  prune(now);

  Counter &counter = m_counters[Key{operation, user_id}];

  if (now >= counter.started_at + m_limit.window) {
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
  if (m_counters.size() < k_prune_threshold) {
    return;
  }

  for (auto entry = m_counters.begin(); entry != m_counters.end();) {
    if (now >= entry->second.started_at + m_limit.window) {
      entry = m_counters.erase(entry);
      continue;
    }

    ++entry;
  }
}

} // namespace dgds::server
