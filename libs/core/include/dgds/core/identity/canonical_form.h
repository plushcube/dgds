#pragma once

#include <string>
#include <string_view>

namespace dgds::core {

using Content = std::string_view;
using CanonicalForm = std::string;

[[nodiscard]] CanonicalForm canonical_form(Content content);

} // namespace dgds::core
