#include <dgds/core/crypto/sealed_content_codec.h>

#include <codec/binary.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace dgds::core {
namespace {

constexpr std::uint64_t k_max_length = std::numeric_limits<std::uint32_t>::max();

} // namespace

Result<ContentBuffer> encode_sealed_content(const SealedContent &sealed) {
  if (sealed.ciphertext.size() > k_max_length) {
    return std::unexpected(CoreError::sealed_content_malformed);
  }

  ContentBuffer data;
  data.reserve(1 + sealed.nonce.size() + k_length_size + sealed.ciphertext.size() + sealed.tag.size());

  append_integer(data, static_cast<std::uint8_t>(sealed.algorithm), 1);
  append_bytes(data, sealed.nonce.data(), sealed.nonce.size());
  append_integer(data, sealed.ciphertext.size(), k_length_size);
  append_bytes(data, sealed.ciphertext.data(), sealed.ciphertext.size());
  append_bytes(data, sealed.tag.data(), sealed.tag.size());

  return data;
}

Result<SealedContent> decode_sealed_content(Content data) {
  Reader reader(data);

  std::uint64_t algorithm = 0;
  std::uint64_t length = 0;
  SealedContent sealed{};

  if (!reader.read_integer(algorithm, 1) || !reader.read_bytes(sealed.nonce.data(), sealed.nonce.size()) ||
      !reader.read_integer(length, k_length_size)) {
    return std::unexpected(CoreError::sealed_content_malformed);
  }

  if (length > reader.remaining()) {
    return std::unexpected(CoreError::sealed_content_malformed);
  }

  sealed.algorithm = static_cast<AeadAlgorithm>(algorithm);
  sealed.ciphertext.resize(static_cast<std::size_t>(length));

  if (!reader.read_bytes(sealed.ciphertext.data(), sealed.ciphertext.size()) ||
      !reader.read_bytes(sealed.tag.data(), sealed.tag.size()) || !reader.empty()) {
    return std::unexpected(CoreError::sealed_content_malformed);
  }

  return sealed;
}

} // namespace dgds::core
