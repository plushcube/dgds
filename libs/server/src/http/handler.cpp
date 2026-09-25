#include <dgds/server/http/handler.h>

#include <dgds/core/models/protocol.h>
#include <dgds/server/api/response.h>

#include <array>
#include <string_view>

namespace dgds::server::http {
namespace {

using api::ProtocolError;
using api::Surface;
using api::SurfaceResult;

using Operation = SurfaceResult (Surface::*)(core::Content);

struct Route {
  std::string_view path;
  Operation operation;
};

constexpr std::array<Route, 10> k_routes{{{"/register", &Surface::register_user},
                                          {"/login", &Surface::log_in},
                                          {"/catalog", &Surface::catalog},
                                          {"/publish", &Surface::publish},
                                          {"/author-publications", &Surface::author_publications},
                                          {"/buy", &Surface::buy},
                                          {"/purchases", &Surface::purchases},
                                          {"/restore-receipt", &Surface::restore_receipt},
                                          {"/context-identity", &Surface::context_identity},
                                          {"/fetch-package", &Surface::fetch_package}}};

struct Status {
  core::Content code;
  int status;
};

const std::array<Status, 26> k_statuses{{
    {api::code_of(ProtocolError::request_malformed), 400},
    {api::code_of(ProtocolError::version_unsupported), 400},
    {api::code_of(ProtocolError::response_malformed), 500},
    {api::code_of(ProtocolError::operation_unknown), 404},
    {api::code_of(ProtocolError::rate_limit_exceeded), 429},
    {core::code_of(core::CoreError::authorization_failed), 401},
    {core::code_of(core::CoreError::not_permitted), 403},
    {core::code_of(core::CoreError::user_not_found), 404},
    {core::code_of(core::CoreError::publication_not_found), 404},
    {core::code_of(core::CoreError::purchase_not_found), 404},
    {core::code_of(core::CoreError::receipt_not_found), 404},
    {core::code_of(core::CoreError::blob_not_found), 404},
    {core::code_of(core::CoreError::key_not_found), 404},
    {core::code_of(core::CoreError::user_name_taken), 409},
    {core::code_of(core::CoreError::content_duplicate), 409},
    {core::code_of(core::CoreError::record_exists), 409},
    {core::code_of(core::CoreError::content_mismatch), 422},
    {core::code_of(core::CoreError::signature_invalid), 422},
    {core::code_of(core::CoreError::authentication_failed), 422},
    {core::code_of(core::CoreError::receipt_version_unsupported), 400},
    {core::code_of(core::CoreError::package_version_unsupported), 400},
    {core::code_of(core::CoreError::algorithm_unsupported), 400},
    {core::code_of(core::CoreError::key_size_mismatch), 400},
    {core::code_of(core::CoreError::key_malformed), 400},
    {core::code_of(core::CoreError::mark_version_unsupported), 400},
    {core::code_of(core::CoreError::storage_failed), 500},
}};

int status_of(core::Content code) {
  for (const Status &entry : k_statuses) {
    if (entry.code == code) {
      return entry.status;
    }
  }

  return 500;
}

HttpResponse reply(const SurfaceResult &result) {
  return HttpResponse{.status = result.accepted ? 200 : status_of(result.code), .body = result.body};
}

} // namespace

HttpResponse handle(Surface &surface, const std::string &path, const std::string &body) {
  for (const Route &route : k_routes) {
    if (route.path == path) {
      return reply((surface.*route.operation)(body));
    }
  }

  return reply(Surface::unknown_operation());
}

} // namespace dgds::server::http
