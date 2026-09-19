#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/crypto.h>
#include <dgds/core/models/errors.h>

#include <filesystem>

namespace dgds::stubs {

[[nodiscard]] core::Result<void> ensure_directory(const std::filesystem::path &path);
[[nodiscard]] core::Result<bool> directory_is_empty(const std::filesystem::path &path);
[[nodiscard]] core::Result<core::ContentBuffer> load_file(const std::filesystem::path &path, core::CoreError missing);
[[nodiscard]] core::Result<void> store_file(const std::filesystem::path &path, core::Content bytes);
[[nodiscard]] core::Result<core::SecretBytes<core::k_key_size>> load_secret(const std::filesystem::path &path);
[[nodiscard]] core::Result<core::SecretBytes<core::k_key_size>>
load_or_create_secret(const std::filesystem::path &path);

} // namespace dgds::stubs
