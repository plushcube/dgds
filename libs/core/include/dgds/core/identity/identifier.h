#pragma once

#include <dgds/core/models/errors.h>

#include <cstdint>

namespace dgds::core {

[[nodiscard]] Result<std::uint64_t> generate_identifier();

} // namespace dgds::core
