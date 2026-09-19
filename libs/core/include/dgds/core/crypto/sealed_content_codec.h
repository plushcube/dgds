#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/errors.h>

namespace dgds::core {

[[nodiscard]] Result<ContentBuffer> encode_sealed_content(const SealedContent &sealed);
[[nodiscard]] Result<SealedContent> decode_sealed_content(Content data);

} // namespace dgds::core
