#pragma once

#include <dgds/core/models/crypto.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/identity.h>

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
