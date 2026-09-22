#pragma once

#include <dgds/core/crypto/secure_buffer.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/envelope.h>
#include <dgds/core/models/errors.h>

namespace dgds::core {

[[nodiscard]] Result<SecureBuffer> open_package(const Package &package, const SymmetricKey &purchase_key);

} // namespace dgds::core
