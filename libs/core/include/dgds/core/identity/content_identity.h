#pragma once

#include <dgds/core/identity/canonical_form.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/identity.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace dgds::core {

[[nodiscard]] Content as_content(const ContentIdentity &identity);
[[nodiscard]] Result<ContentIdentity> content_identity(Content content);
[[nodiscard]] std::string to_hex(const std::uint8_t *bytes, std::size_t size);
[[nodiscard]] std::string to_hex(const ContentIdentity &identity);

} // namespace dgds::core
