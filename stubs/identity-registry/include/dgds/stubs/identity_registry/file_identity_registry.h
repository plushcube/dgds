#pragma once

#include <dgds/core/ports/identity_registry.h>

#include <filesystem>
#include <utility>

namespace dgds::stubs {

class FileIdentityRegistry : public core::IdentityRegistry {
public:
  explicit FileIdentityRegistry(std::filesystem::path root) : m_root(std::move(root)) {}

  [[nodiscard]] core::Result<core::ClaimOutcome> claim(const core::ContentIdentity &identity) override;

private:
  std::filesystem::path m_root;
};

} // namespace dgds::stubs
