#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/package.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/symmetric_key.h>

namespace dgds::core {

[[nodiscard]] Result<ContentBuffer> open_package(const Package &package, const SymmetricKey &purchase_key);

} // namespace dgds::core
