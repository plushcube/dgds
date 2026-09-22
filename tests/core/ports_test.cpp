#include <dgds/core/models/crypto.h>
#include <dgds/core/models/device.h>
#include <dgds/core/models/errors.h>
#include <dgds/core/ports/blob_store.h>
#include <dgds/core/ports/identity_registry.h>
#include <dgds/core/ports/key_store.h>
#include <dgds/core/ports/metadata_registry.h>

#include <cstddef>
#include <type_traits>

namespace {

using dgds::core::BlobStore;
using dgds::core::IdentityRegistry;
using dgds::core::KeyStore;
using dgds::core::MetadataRegistry;
using dgds::core::Result;
using dgds::core::SecretBytes;

template <typename Signature> struct Returned;

template <typename Port, typename Value, typename... Arguments> struct Returned<Value (Port::*)(Arguments...)> {
  using Type = Value;
};

template <typename Value> struct Unwrapped {
  using Type = Value;
};

template <typename Value> struct Unwrapped<Result<Value>> {
  using Type = Value;
};

template <typename Value> struct IsSecretKey : std::false_type {};

template <std::size_t Size> struct IsSecretKey<SecretBytes<Size>> : std::true_type {};

template <auto Operation>
inline constexpr bool k_returns_no_key =
    !IsSecretKey<typename Unwrapped<typename Returned<decltype(Operation)>::Type>::Type>::value;

static_assert(k_returns_no_key<&BlobStore::store>);
static_assert(k_returns_no_key<&BlobStore::load>);

static_assert(k_returns_no_key<&KeyStore::seal>);
static_assert(k_returns_no_key<&KeyStore::open>);
static_assert(k_returns_no_key<&KeyStore::wrap>);

static_assert(k_returns_no_key<&MetadataRegistry::add_user>);
static_assert(k_returns_no_key<&MetadataRegistry::find_user>);
static_assert(k_returns_no_key<&MetadataRegistry::find_user_by_name>);
static_assert(k_returns_no_key<&MetadataRegistry::add_publication>);
static_assert(k_returns_no_key<&MetadataRegistry::find_publication>);
static_assert(k_returns_no_key<&MetadataRegistry::publications>);
static_assert(k_returns_no_key<&MetadataRegistry::publications_of_author>);
static_assert(k_returns_no_key<&MetadataRegistry::add_purchase>);
static_assert(k_returns_no_key<&MetadataRegistry::find_purchase>);
static_assert(k_returns_no_key<&MetadataRegistry::find_purchase_of>);
static_assert(k_returns_no_key<&MetadataRegistry::purchases_of_user>);
static_assert(k_returns_no_key<&MetadataRegistry::purchase_count>);
static_assert(k_returns_no_key<&MetadataRegistry::save_receipt>);
static_assert(k_returns_no_key<&MetadataRegistry::find_receipt>);

static_assert(k_returns_no_key<&IdentityRegistry::claim>);

} // namespace
