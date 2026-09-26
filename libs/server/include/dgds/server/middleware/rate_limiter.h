#pragma once

#include <dgds/core/models/timestamp.h>
#include <dgds/core/models/user.h>

#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace dgds::server {

struct RateLimit {
  std::size_t calls = 0;
  core::Timestamp window = 0;
};

enum class LimitedOperation {
  request,
  publication,
  delivery,
};

class RateLimiter {
public:
  RateLimiter() = default;
  explicit RateLimiter(RateLimit limit) : m_limit(limit) {}

  [[nodiscard]] bool accepted(LimitedOperation operation, const core::UserId &user_id, core::Timestamp now);
  [[nodiscard]] bool accepted(LimitedOperation operation, std::string_view key, core::Timestamp now);

private:
  struct Counter {
    core::Timestamp started_at = 0;
    std::size_t calls = 0;
  };

  using Key = std::pair<LimitedOperation, std::string>;
  using Counters = std::map<Key, Counter>;

  static constexpr std::size_t k_prune_threshold = 1024;
  static constexpr std::size_t k_prune_interval = 256;

  void prune(core::Timestamp now);

  RateLimit m_limit;
  std::size_t m_calls_since_prune = 0;
  std::mutex m_mutex;
  Counters m_counters;
};

} // namespace dgds::server
