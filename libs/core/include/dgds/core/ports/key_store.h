#pragma once

#include <dgds/core/crypto/secure_buffer.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/content_identity.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/sealed_content.h>
#include <dgds/core/models/symmetric_key.h>

namespace dgds::core {

class KeyStore {
public:
  KeyStore() = default;
  KeyStore(const KeyStore &) = delete;
  KeyStore &operator=(const KeyStore &) = delete;
  KeyStore(KeyStore &&) = delete;
  KeyStore &operator=(KeyStore &&) = delete;
  virtual ~KeyStore() = default;

  [[nodiscard]] virtual Result<SealedContent> seal(const ContentIdentity &identity, const SecureBuffer &plaintext,
                                                   Content associated_data) = 0;
  [[nodiscard]] virtual Result<SecureBuffer> open(const ContentIdentity &identity, const SealedContent &sealed,
                                                  Content associated_data) = 0;
  [[nodiscard]] virtual Result<SealedContent> wrap(const ContentIdentity &identity, const SymmetricKey &purchase_key,
                                                   Content associated_data) = 0;
};

} // namespace dgds::core
