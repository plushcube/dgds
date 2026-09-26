#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <dgds/stubs/metadata_registry/file_metadata_registry.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

using dgds::core::CoreError;
using dgds::core::Credentials;
using dgds::core::UserId;
using dgds::server::SessionStore;
using dgds::server::UserService;
using dgds::stubs::FileMetadataRegistry;

constexpr std::uint8_t k_uuid_version = 4;
constexpr std::uint8_t k_uuid_version_byte = 6;
constexpr std::uint8_t k_uuid_variant_byte = 8;

class UserServiceTest : public ::testing::Test {
protected:
  UserServiceTest() : m_root(temporary_root()) { std::filesystem::create_directories(m_root); }

  void TearDown() override { std::filesystem::remove_all(m_root); }

  static std::filesystem::path temporary_root() {
    return std::filesystem::temp_directory_path() /
           ("dgds-user-service-" + std::to_string(::getpid()) + "-" + std::to_string(counter++));
  }

  std::filesystem::path m_root;
  FileMetadataRegistry m_metadata{m_root};
  SessionStore m_sessions;
  UserService m_service{m_metadata, m_sessions};

private:
  static inline std::atomic<unsigned> counter{0};
};

TEST_F(UserServiceTest, RegistersUserWithUuid) {
  const auto account = m_service.register_user("автор");

  ASSERT_TRUE(account.has_value());
  EXPECT_EQ(account->name, "автор");
  EXPECT_EQ(account->user_id.size(), UserId{}.size());
  EXPECT_EQ(account->user_id[k_uuid_version_byte] >> 4, k_uuid_version);
  EXPECT_EQ(account->user_id[k_uuid_variant_byte] & 0xC0, 0x80);

  const auto found = m_metadata.find_user_by_name("автор");

  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->user_id, account->user_id);
}

TEST_F(UserServiceTest, KeepsIdentifiersUnique) {
  std::set<UserId> identifiers;

  for (std::size_t index = 0; index < 16; ++index) {
    const auto account = m_service.register_user("пользователь-" + std::to_string(index));

    ASSERT_TRUE(account.has_value());
    identifiers.insert(account->user_id);
  }

  EXPECT_EQ(identifiers.size(), 16U);
}

TEST_F(UserServiceTest, RejectsTakenName) {
  const auto first = m_service.register_user("автор");
  ASSERT_TRUE(first.has_value());

  const auto second = m_service.register_user("автор");

  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error(), CoreError::user_name_taken);

  const auto found = m_metadata.find_user_by_name("автор");

  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->user_id, first->user_id);
}

TEST_F(UserServiceTest, IssuesCredentialsOnLogin) {
  const auto account = m_service.register_user("покупатель");
  ASSERT_TRUE(account.has_value());

  const auto credentials = m_service.log_in("покупатель");

  ASSERT_TRUE(credentials.has_value());
  EXPECT_EQ(credentials->user_id, account->user_id);
  EXPECT_FALSE(credentials->token.empty());

  const auto resolved = m_sessions.resolve(credentials->token);

  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), account->user_id);
}

TEST_F(UserServiceTest, RejectsUnknownName) {
  const auto credentials = m_service.log_in("никого");

  ASSERT_FALSE(credentials.has_value());
  EXPECT_EQ(credentials.error(), CoreError::user_not_found);
}

TEST_F(UserServiceTest, ReplacesTokenOnRepeatedLogIn) {
  const auto account = m_service.register_user("автор");
  ASSERT_TRUE(account.has_value());

  const auto first = m_service.log_in("автор");
  const auto second = m_service.log_in("автор");

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(first->token, second->token);

  const auto stale = m_sessions.resolve(first->token);
  const auto current = m_sessions.resolve(second->token);

  ASSERT_FALSE(stale.has_value()) << "прежний токен должен перестать действовать";
  EXPECT_EQ(stale.error(), CoreError::authorization_failed);

  ASSERT_TRUE(current.has_value());
  EXPECT_EQ(current.value(), account->user_id);
}

TEST_F(UserServiceTest, KeepsOtherUsersSessionsOnRepeatedLogIn) {
  const auto first = m_service.register_user("первый");
  const auto second = m_service.register_user("второй");
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  const auto first_credentials = m_service.log_in("первый");
  const auto second_credentials = m_service.log_in("второй");
  ASSERT_TRUE(first_credentials.has_value());
  ASSERT_TRUE(second_credentials.has_value());

  const auto renewed = m_service.log_in("первый");
  ASSERT_TRUE(renewed.has_value());

  EXPECT_FALSE(m_sessions.resolve(first_credentials->token).has_value());
  EXPECT_TRUE(m_sessions.resolve(renewed->token).has_value());
  EXPECT_TRUE(m_sessions.resolve(second_credentials->token).has_value())
      << "сессия другого пользователя не должна пострадать";
}

TEST_F(UserServiceTest, StopsResolvingExpiredSession) {
  SessionStore sessions{std::chrono::seconds{1}};

  const auto account = m_service.register_user("автор");
  ASSERT_TRUE(account.has_value());

  const auto credentials = sessions.issue(account->user_id);
  ASSERT_TRUE(credentials.has_value());
  ASSERT_TRUE(sessions.resolve(credentials->token).has_value());

  std::this_thread::sleep_for(std::chrono::milliseconds{1100});

  const auto expired = sessions.resolve(credentials->token);

  ASSERT_FALSE(expired.has_value());
  EXPECT_EQ(expired.error(), CoreError::authorization_failed);
}

TEST_F(UserServiceTest, RejectsUnknownToken) {
  const auto account = m_service.register_user("автор");
  ASSERT_TRUE(account.has_value());

  const auto resolved = m_sessions.resolve("чужой-токен");

  ASSERT_FALSE(resolved.has_value());
  EXPECT_EQ(resolved.error(), CoreError::authorization_failed);
}

} // namespace
