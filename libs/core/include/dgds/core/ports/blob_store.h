#pragma once

#include <dgds/core/models/content_identity.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/sealed_content.h>

namespace dgds::core {

class BlobStore {
public:
  BlobStore() = default;
  BlobStore(const BlobStore &) = delete;
  BlobStore &operator=(const BlobStore &) = delete;
  BlobStore(BlobStore &&) = delete;
  BlobStore &operator=(BlobStore &&) = delete;
  virtual ~BlobStore() = default;

  [[nodiscard]] virtual Result<void> store(const ContentIdentity &identity, const SealedContent &blob) = 0;
  [[nodiscard]] virtual Result<SealedContent> load(const ContentIdentity &identity) = 0;
};

} // namespace dgds::core
