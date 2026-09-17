#pragma once

#include <dgds/core/models/author_key.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/content_identity.h>
#include <dgds/core/models/sealed_content.h>
#include <dgds/core/models/signature.h>
#include <dgds/core/models/signature_algorithm.h>

#include <cstdint>

namespace dgds::core {

inline constexpr std::uint8_t k_package_version = 1;

struct Package {
  std::uint8_t version;
  SealedContent content;
  SealedContent wrapped_blob_key;
  ContentIdentity identity;
  AuthorPublicKey author_key;
  ContentBuffer author_name;
  SignatureAlgorithm signature_algorithm;
  Signature signature;
};

} // namespace dgds::core
