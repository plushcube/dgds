#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/errors.h>

#include <filesystem>
#include <vector>

namespace dgds::stubs {

[[nodiscard]] core::Result<void> ensure_directory(const std::filesystem::path &path);
[[nodiscard]] core::Result<bool> claim_file(const std::filesystem::path &path);
[[nodiscard]] core::Result<bool> file_exists(const std::filesystem::path &path);
[[nodiscard]] core::Result<std::vector<std::filesystem::path>> list_files(const std::filesystem::path &directory);
[[nodiscard]] core::Result<core::ContentBuffer> load_file(const std::filesystem::path &path, core::CoreError missing);
[[nodiscard]] core::Result<void> store_file(const std::filesystem::path &path, core::Content bytes);
[[nodiscard]] core::Result<bool> store_file_if_absent(const std::filesystem::path &path, core::Content bytes);
[[nodiscard]] core::Result<void> remove_file(const std::filesystem::path &path);
[[nodiscard]] core::Result<core::SecretBytes<core::k_key_size>> load_secret(const std::filesystem::path &path);
[[nodiscard]] core::Result<core::SecretBytes<core::k_key_size>>
load_or_create_secret(const std::filesystem::path &path);

} // namespace dgds::stubs
