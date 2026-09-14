#pragma once

#include <dgds/core/models/core_error.h>

#include <expected>

namespace dgds::core {

template <typename Value> using Result = std::expected<Value, CoreError>;

} // namespace dgds::core
