#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/result.h>
#include <dgds/core/models/sealed_content.h>
#include <dgds/core/models/symmetric_key.h>

namespace dgds::core {

inline constexpr AeadAlgorithm k_aead_algorithm = AeadAlgorithm::aes_256_gcm;

[[nodiscard]] Result<SealedContent> encrypt(Content plaintext, const SymmetricKey &key, Content associated_data);
[[nodiscard]] Result<ContentBuffer> decrypt(const SealedContent &sealed, const SymmetricKey &key,
                                            Content associated_data);

} // namespace dgds::core
