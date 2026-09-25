#pragma once

#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

#include <cstddef>
#include <map>
#include <mutex>
#include <utility>

namespace dgds::server {

struct RateLimit {
  std::size_t calls = 0;
  core::Timestamp window = 0;
};

enum class LimitedOperation {
  publication,
  delivery,
};

class RateLimiter {
public:
  RateLimiter() = default;
  explicit RateLimiter(RateLimit limit) : m_limit(limit) {}

  [[nodiscard]] bool accepted(LimitedOperation operation, const core::UserId &user_id, core::Timestamp now);

private:
  struct Counter {
    core::Timestamp started_at = 0;
    std::size_t calls = 0;
  };

  using Key = std::pair<LimitedOperation, core::UserId>;
  using Counters = std::map<Key, Counter>;

  static constexpr std::size_t k_prune_threshold = 1024;

  void prune(core::Timestamp now);

  RateLimit m_limit;
  std::mutex m_mutex;
  Counters m_counters;
};

} // namespace dgds::server
