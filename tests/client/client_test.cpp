#include <client_fixture.h>

#include <dgds/core/identity/canonical_form.h>

#include <gtest/gtest.h>

#include <string>

namespace {

using dgds::client::ApiClient;
using dgds::client::ApiTransport;
using dgds::client::AuthorPublicKey;
using dgds::client::Content;
using dgds::client::ContentIdentity;
using dgds::client::Credentials;
using dgds::client::DevicePublicKey;
using dgds::client::Package;
using dgds::client::PublicationDraft;
using dgds::client::PublicationId;
using dgds::client::PublicationSummaries;
using dgds::client::PurchaseId;
using dgds::client::PurchaseSummaries;
using dgds::client::Receipt;
using dgds::client::Result;
using dgds::client::Signature;
using dgds::client::UserAccount;
using dgds::core::canonical_form;
using dgds::core::CoreError;
using dgds::core::generate_author_key;
using dgds::core::PublicationDraft;
using dgds::test::ClientFixture;
using dgds::test::k_client_title;
using dgds::test::long_text;

TEST_F(ClientFixture, RunsFullPurchaseScenario) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto catalog = m_client.catalog();
  ASSERT_TRUE(catalog.has_value());
  ASSERT_EQ(catalog->size(), 1U);
  EXPECT_EQ(catalog->front().publication_id, publication_id);

  const auto receipt = m_client.buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());

  const auto purchases = m_client.purchases(buyer);
  ASSERT_TRUE(purchases.has_value());
  ASSERT_EQ(purchases->size(), 1U);
  EXPECT_EQ(purchases->front().publication.publication_id, publication_id);
  EXPECT_EQ(purchases->front().publication.title, std::string(k_client_title));

  const auto content = m_client.fetch_content(buyer, receipt->header.purchase_id);

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(ClientFixture, RecoversReceiptOnAnotherDevice) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto buyer = sign_in("покупатель");

  const auto receipt = m_client.buy(buyer, publication_id);
  ASSERT_TRUE(receipt.has_value());

  const auto content = m_second_client.fetch_content(buyer, receipt->header.purchase_id);

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);

  const auto stored = m_second_receipts.load(receipt->header.purchase_id);
  ASSERT_TRUE(stored.has_value());
}

TEST_F(ClientFixture, AuthorReceivesOwnPublication) {
  const std::string text = long_text();
  const auto publication_id = publish("автор", text);
  const auto author = m_client.log_in("автор");
  ASSERT_TRUE(author.has_value());

  const auto receipt = m_client.buy(author.value(), publication_id);

  ASSERT_TRUE(receipt.has_value());
  EXPECT_EQ(receipt->header.purchase_id, publication_id);

  const auto purchases = m_client.purchases(author.value());
  ASSERT_TRUE(purchases.has_value());
  EXPECT_TRUE(purchases->empty());

  const auto content = m_client.fetch_content(author.value(), publication_id);

  ASSERT_TRUE(content.has_value());
  EXPECT_EQ(canonical_form(content->view()), text);
}

TEST_F(ClientFixture, RefusesCredentialsOfAnotherUser) {
  const std::string text = long_text();
  const auto other = sign_in("другой");
  const auto author = m_client.register_user("автор");
  const auto credentials = m_client.log_in("автор");
  ASSERT_TRUE(author.has_value());
  ASSERT_TRUE(credentials.has_value());

  const auto keys = generate_author_key();
  const auto identity = dgds::core::content_identity(text);
  ASSERT_TRUE(keys.has_value());
  ASSERT_TRUE(identity.has_value());

  const auto signature = dgds::core::sign_author(identity.value(), "автор", keys->private_key);
  ASSERT_TRUE(signature.has_value());

  const Credentials forged{.user_id = other.user_id, .token = credentials->token};
  const PublicationDraft draft{.title = std::string(k_client_title), .file_name = "файл.txt", .content = text};

  const auto publication = m_client.publish(forged, draft, keys->public_key, signature.value());

  ASSERT_FALSE(publication.has_value());
  EXPECT_EQ(publication.error(), CoreError::authorization_failed);
}

class SwappingTransport : public dgds::client::ApiTransport {
public:
  SwappingTransport(ApiTransport &inner, PurchaseId from, PurchaseId to) : m_inner(inner), m_from(from), m_to(to) {}

  Result<UserAccount> register_user(Content name) override { return m_inner.register_user(name); }

  Result<Credentials> log_in(Content name) override { return m_inner.log_in(name); }

  Result<PublicationSummaries> catalog() override { return m_inner.catalog(); }

  Result<PublicationId> publish(const Credentials &credentials, const PublicationDraft &draft,
                                const AuthorPublicKey &author_key, const Signature &signature) override {
    return m_inner.publish(credentials, draft, author_key, signature);
  }

  Result<Receipt> buy(const Credentials &credentials, const PublicationId &publication_id,
                      const DevicePublicKey &device_key) override {
    return m_inner.buy(credentials, publication_id, device_key);
  }

  Result<PurchaseSummaries> purchases(const Credentials &credentials) override {
    return m_inner.purchases(credentials);
  }

  Result<Receipt> restore_receipt(const Credentials &credentials, const PurchaseId &purchase_id,
                                  const DevicePublicKey &device_key) override {
    return m_inner.restore_receipt(credentials, purchase_id, device_key);
  }

  Result<ContentIdentity> context_identity(const Credentials &credentials, const PurchaseId &context_id) override {
    return m_inner.context_identity(credentials, context_id);
  }

  Result<Package> fetch_package(const Credentials &credentials, const PurchaseId &purchase_id,
                                const DevicePublicKey &device_key) override {
    return m_inner.fetch_package(credentials, purchase_id == m_from ? m_to : purchase_id, device_key);
  }

private:
  ApiTransport &m_inner;
  PurchaseId m_from;
  PurchaseId m_to;
};

TEST_F(ClientFixture, RefusesPackageOfAnotherPublication) {
  const std::string base = long_text();
  const std::string first_text = "первая публикация\n" + base;
  const std::string second_text = "вторая публикация\n" + base;

  const auto first = publish("автор", first_text);
  const auto second = publish("автор", second_text);
  const auto buyer = sign_in("покупатель");

  dgds::stubs::FileDeviceKey device_key{m_root / "swapped.key"};
  dgds::stubs::FileReceiptStore receipts{m_root / "swapped-receipts"};
  ApiClient buying{m_transport, device_key, receipts};

  const auto first_receipt = buying.buy(buyer, first);
  const auto second_receipt = buying.buy(buyer, second);
  ASSERT_TRUE(first_receipt.has_value());
  ASSERT_TRUE(second_receipt.has_value());

  SwappingTransport swapping{m_transport, first_receipt->header.purchase_id, second_receipt->header.purchase_id};
  ApiClient client{swapping, device_key, receipts};

  const auto wrong = client.fetch_content(buyer, first_receipt->header.purchase_id);

  ASSERT_FALSE(wrong.has_value());
  EXPECT_EQ(wrong.error(), CoreError::content_mismatch);

  const auto honest = client.fetch_content(buyer, second_receipt->header.purchase_id);

  ASSERT_TRUE(honest.has_value());
  EXPECT_EQ(canonical_form(honest->view()), second_text);
}

} // namespace
