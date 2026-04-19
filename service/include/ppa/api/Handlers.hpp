#pragma once

#include <httplib.h>

#include "ppa/core/CritiqueService.hpp"

namespace ppa::api {

nlohmann::json health_payload();
CapabilitiesResponse capabilities_payload(const CritiqueService& service);
CritiqueResponse critique_payload(const CritiqueService& service, const CritiqueRequest& request);
void register_routes(httplib::Server& server, CritiqueService& service);

}  // namespace ppa::api
