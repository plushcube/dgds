#include <client_fixture.h>

#include <dgds/core/identity/canonical_form.h>

#include <gtest/gtest.h>

#include <string>

namespace {

using dgds::core::canonical_form;
using dgds::core::CoreError;
using dgds::core::Credentials;
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

} // namespace
