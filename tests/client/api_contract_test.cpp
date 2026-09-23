#include <client_fixture.h>

#include <dgds/core/identity/canonical_form.h>
#include <dgds/stubs/device_key/file_device_key.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <expected>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using dgds::client::ApiClient;
using dgds::client::Content;
using dgds::client::ContentBuffer;
using dgds::client::PurchaseId;
using dgds::client::Result;
using dgds::core::canonical_form;
using dgds::core::CoreError;
using dgds::test::ClientFixture;
using dgds::test::k_client_line;
using dgds::test::long_text;

template <typename Type> struct carries_key_material : std::false_type {};

template <std::size_t Size> struct carries_key_material<dgds::core::SecretBytes<Size>> : std::true_type {};

template <typename Type> struct result_carries_key_material : carries_key_material<Type> {};

template <typename Type> struct result_carries_key_material<dgds::core::Result<Type>> : carries_key_material<Type> {};

template <typename Type> constexpr bool carries_key_material_v = result_carries_key_material<Type>::value;

static_assert(!carries_key_material_v<decltype(std::declval<ApiClient &>().register_user(Content{}))>,
              "регистрация не должна возвращать ключевой материал");
static_assert(!carries_key_material_v<decltype(std::declval<ApiClient &>().log_in(Content{}))>,
              "вход не должен возвращать ключевой материал");
static_assert(!carries_key_material_v<decltype(std::declval<ApiClient &>().catalog())>,
              "каталог не должен возвращать ключевой материал");
static_assert(!carries_key_material_v<decltype(std::declval<ApiClient &>().publish(
                  std::declval<const dgds::core::Credentials &>(), std::declval<const dgds::core::PublicationDraft &>(),
                  std::declval<const dgds::core::AuthorPublicKey &>(), std::declval<const dgds::core::Signature &>()))>,
              "публикация не должна возвращать ключевой материал");
static_assert(!carries_key_material_v<
                  decltype(std::declval<ApiClient &>().purchases(std::declval<const dgds::core::Credentials &>()))>,
              "список покупок не должен возвращать ключевой материал");
static_assert(!carries_key_material_v<decltype(std::declval<ApiClient &>().buy(
                  std::declval<const dgds::core::Credentials &>(), std::declval<const dgds::core::PublicationId &>()))>,
              "покупка не должна возвращать ключевой материал");
static_assert(!carries_key_material_v<decltype(std::declval<ApiClient &>().fetch_content(
                  std::declval<const dgds::core::Credentials &>(), std::declval<const PurchaseId &>()))>,
              "получение контента должно возвращать содержимое, а не ключ");

class InMemoryReceiptStore : public dgds::client::ReceiptStore {
public:
  Result<void> save(const PurchaseId &purchase_id, Content blob) override {
    m_saved.emplace_back(blob);
    m_blobs.insert_or_assign(purchase_id, std::string(blob));

    return {};
  }

  Result<ContentBuffer> load(const PurchaseId &purchase_id) override {
    const auto found = m_blobs.find(purchase_id);

    if (found == m_blobs.end()) {
      return std::unexpected(CoreError::receipt_not_found);
    }

    return found->second;
  }

  [[nodiscard]] const std::vector<std::string> &saved() const { return m_saved; }

private:
  std::vector<std::string> m_saved;
  std::map<PurchaseId, std::string> m_blobs;
};

std::string read_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();

  return buffer.str();
}

TEST_F(ClientFixture, WorksWithForeignReceiptStore) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  InMemoryReceiptStore store;
  dgds::stubs::FileDeviceKey device_key{m_root / "foreign.key"};
  ApiClient client{m_transport, device_key, store};

  const auto receipt = client.buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());
  ASSERT_EQ(store.saved().size(), 1U);

  const std::string &blob = store.saved().front();
  EXPECT_EQ(canonical_form(blob).find(k_client_line), std::string::npos);

  const std::string private_key = read_file(m_root / "foreign.key");
  ASSERT_FALSE(private_key.empty());
  EXPECT_EQ(blob.find(private_key), std::string::npos);

  const auto content = client.fetch_content(buyer, receipt->header.purchase_id);
  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);
}

} // namespace
