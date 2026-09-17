#pragma once

#include <dgds/core/models/content.h>

#include <string>

namespace dgds::core {

using CanonicalForm = std::string;

[[nodiscard]] CanonicalForm canonical_form(Content content);

} // namespace dgds::core
