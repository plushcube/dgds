#pragma once

#include <dgds/core/models/author.h>
#include <dgds/core/models/content.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/models/identity.h>

namespace dgds::core {

inline constexpr SignatureAlgorithm k_signature_algorithm = SignatureAlgorithm::ed25519;

[[nodiscard]] Result<AuthorKeyPair> generate_author_key();
[[nodiscard]] Result<Signature> sign_author(const ContentIdentity &identity, Content author_name,
                                            const AuthorPrivateKey &key);
[[nodiscard]] Result<bool> verify_author(const ContentIdentity &identity, Content author_name,
                                         const Signature &signature, const AuthorPublicKey &key);

} // namespace dgds::core
