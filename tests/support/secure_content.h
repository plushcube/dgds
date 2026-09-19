#pragma once

#include <dgds/core/crypto/secure_buffer.h>
#include <dgds/core/models/content.h>

#include <cstring>

namespace dgds::test {

inline core::SecureBuffer make_plaintext(core::Content text) {
  core::SecureBuffer buffer(text.size());

  if (text.size() > 0) {
    std::memcpy(buffer.data(), text.data(), text.size());
  }

  return buffer;
}

} // namespace dgds::test
