#pragma once

#include <dgds/core/models/content.h>
#include <dgds/core/models/timestamp.h>
#include <dgds/server/api/errors.h>
#include <dgds/server/api/response.h>
#include <dgds/server/middleware/rate_limiter.h>
#include <dgds/server/services/catalog_service.h>
#include <dgds/server/services/delivery_service.h>
#include <dgds/server/services/publication_service.h>
#include <dgds/server/services/purchase_service.h>
#include <dgds/server/services/session_store.h>
#include <dgds/server/services/user_service.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace dgds::server::api {

class Surface {
public:
  using Clock = std::function<core::Timestamp()>;

  Surface(UserService &users, SessionStore &sessions, CatalogService &catalog, PublicationService &publications,
          PurchaseService &purchases, DeliveryService &delivery, RateLimiter &limiter, Clock clock)
      : m_users(users), m_sessions(sessions), m_catalog(catalog), m_publications(publications), m_purchases(purchases),
        m_delivery(delivery), m_limiter(limiter), m_clock(std::move(clock)) {}

  [[nodiscard]] SurfaceResult register_user(core::Content body);
  [[nodiscard]] SurfaceResult log_in(core::Content body);
  [[nodiscard]] SurfaceResult catalog(core::Content body);
  [[nodiscard]] SurfaceResult publish(core::Content body);
  [[nodiscard]] SurfaceResult author_publications(core::Content body);
  [[nodiscard]] SurfaceResult buy(core::Content body);
  [[nodiscard]] SurfaceResult purchases(core::Content body);
  [[nodiscard]] SurfaceResult restore_receipt(core::Content body);
  [[nodiscard]] SurfaceResult context_identity(core::Content body);
  [[nodiscard]] SurfaceResult fetch_package(core::Content body);

  [[nodiscard]] static SurfaceResult unknown_operation();

private:
  [[nodiscard]] std::optional<ProtocolError> version_failure(core::Content body);

  [[nodiscard]] core::Result<core::UserId> authorized(const core::Credentials &credentials);

  UserService &m_users;
  SessionStore &m_sessions;
  CatalogService &m_catalog;
  PublicationService &m_publications;
  PurchaseService &m_purchases;
  DeliveryService &m_delivery;
  RateLimiter &m_limiter;
  Clock m_clock;
};

} // namespace dgds::server::api
