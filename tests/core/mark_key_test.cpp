#include <dgds/core/watermark/mark_key.h>

#include <dgds/core/crypto/aead.h>
#include <dgds/core/crypto/sealed_content_codec.h>
#include <dgds/core/models/mark.h>
#include <dgds/core/watermark/mark_channel.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

using dgds::core::derive_mark_key;
using dgds::core::embed_mark;
using dgds::core::encode_sealed_content;
using dgds::core::encrypt;
using dgds::core::k_mark_version;
using dgds::core::Mark;
using dgds::core::SymmetricKey;

constexpr std::string_view k_associated_data = "публикация";
constexpr std::size_t k_secret_window = 8;

SymmetricKey make_key(std::uint8_t seed) {
  SymmetricKey key{};

  for (std::size_t index = 0; index < key.size(); ++index) {
    key.data()[index] = static_cast<std::uint8_t>(seed + index);
  }

  return key;
}

std::string long_text(std::size_t lines) {
  std::string text;

  for (std::size_t index = 0; index < lines; ++index) {
    text += "line " + std::to_string(index) + " of the publication\n";
  }

  return text;
}

bool carries_secret(std::string_view delivered, const SymmetricKey &key) {
  for (std::size_t offset = 0; offset + k_secret_window <= key.size(); ++offset) {
    const std::string_view window(reinterpret_cast<const char *>(key.data()) + offset, k_secret_window);

    if (delivered.find(window) != std::string_view::npos) {
      return true;
    }
  }

  return false;
}

TEST(MarkKey, IsDeterministic) {
  const SymmetricKey master_key = make_key(1);

  const auto first = derive_mark_key(master_key);
  const auto second = derive_mark_key(master_key);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_TRUE(first->equals(second.value()));
}

TEST(MarkKey, DiffersBetweenMasterKeys) {
  const auto first = derive_mark_key(make_key(1));
  const auto second = derive_mark_key(make_key(2));

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_FALSE(first->equals(second.value()));
}

TEST(MarkKey, DiffersFromFileWrapKey) {
  const SymmetricKey file_wrap_key = make_key(40);

  const auto mark_key = derive_mark_key(file_wrap_key);

  ASSERT_TRUE(mark_key.has_value());
  EXPECT_FALSE(mark_key->equals(file_wrap_key));
}

TEST(MarkKey, SecretDoesNotReachDeliveredContent) {
  const SymmetricKey master_key = make_key(7);
  const auto mark_key = derive_mark_key(master_key);
  ASSERT_TRUE(mark_key.has_value());

  const SymmetricKey file_key = make_key(90);
  const std::string text = long_text(10);

  const auto marked = embed_mark(text, Mark{.purchase_id = 4242, .version = k_mark_version});
  ASSERT_TRUE(marked.has_value());
  ASSERT_GT(marked->size(), text.size());

  const auto sealed = encrypt(marked.value(), file_key, k_associated_data);
  ASSERT_TRUE(sealed.has_value());

  const auto encoded = encode_sealed_content(sealed.value());
  ASSERT_TRUE(encoded.has_value());

  EXPECT_FALSE(carries_secret(marked.value(), master_key)) << "мастер-ключ не должен попадать в выдаваемый контент";
  EXPECT_FALSE(carries_secret(marked.value(), mark_key.value()))
      << "ключ метки не должен попадать в выдаваемый контент";
  EXPECT_FALSE(carries_secret(encoded.value(), master_key)) << "мастер-ключ не должен попадать в выдаваемый контент";
  EXPECT_FALSE(carries_secret(encoded.value(), mark_key.value()))
      << "ключ метки не должен попадать в выдаваемый контент";
}

} // namespace
