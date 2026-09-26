#pragma once

#include <dgds/core/models/errors.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace dgds::core {

inline constexpr std::uint8_t k_protocol_version = 1;

inline constexpr std::size_t k_default_page_size = 20;

inline constexpr std::size_t k_max_content_bytes = 4 * 1024 * 1024;
inline constexpr std::size_t k_max_request_bytes = k_max_content_bytes + 4096;
inline constexpr std::size_t k_max_response_bytes = 8 * k_max_content_bytes + 64 * 1024;

static_assert(k_max_request_bytes > k_max_content_bytes);
static_assert(k_max_response_bytes > 8 * k_max_content_bytes);

inline constexpr std::string_view k_request_malformed_code = "request_malformed";
inline constexpr std::string_view k_operation_unknown_code = "operation_unknown";
inline constexpr std::string_view k_response_malformed_code = "response_malformed";
inline constexpr std::string_view k_failure_code = "failure";

[[nodiscard]] std::string_view code_of(CoreError error);
[[nodiscard]] std::optional<CoreError> error_of_code(std::string_view code);

} // namespace dgds::core
