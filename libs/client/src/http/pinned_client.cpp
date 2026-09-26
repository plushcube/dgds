#include <dgds/client/http/pinned_client.h>

#include <dgds/core/models/protocol.h>

#include <httplib.h>

#include <expected>
#include <filesystem>
#include <system_error>
#include <vector>

namespace dgds::client {
namespace {

httplib::SSLVerifierResponse check_pin(const ServerPin &pin, httplib::tls::session_t session) {
  const httplib::tls::cert_t certificate = httplib::tls::get_peer_cert(session);

  if (certificate == nullptr) {
    return httplib::SSLVerifierResponse::CertificateRejected;
  }

  std::vector<unsigned char> der;
  const bool encoded = httplib::tls::get_cert_der(certificate, der);
  httplib::tls::free_cert(certificate);

  if (!encoded) {
    return httplib::SSLVerifierResponse::CertificateRejected;
  }

  return pinned_matches(pin, der) ? httplib::SSLVerifierResponse::NoDecisionMade
                                  : httplib::SSLVerifierResponse::CertificateRejected;
}

} // namespace

core::Result<HttpClient> make_pinned_client(const Endpoint &endpoint, const ServerTrust &trust) {
  std::error_code status;

  if (!std::filesystem::is_regular_file(trust.certificate, status)) {
    return std::unexpected(core::CoreError::storage_failed);
  }

  HttpClient client = std::make_unique<httplib::SSLClient>(endpoint.host, endpoint.port);

  if (client->tls_context() == nullptr) {
    return std::unexpected(core::CoreError::crypto_failed);
  }

  client->set_payload_max_length(core::k_max_response_bytes);
  client->set_ca_cert_path(trust.certificate.string());
  client->enable_server_certificate_verification(true);

  const ServerPin pin = trust.pin;
  client->set_session_verifier([pin](httplib::tls::session_t session) { return check_pin(pin, session); });

  return client;
}

} // namespace dgds::client
