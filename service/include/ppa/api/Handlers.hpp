#pragma once

#include <httplib.h>

#include "ppa/model.hpp"

namespace ppa::api {

nlohmann::json health_payload();
CapabilitiesResponse capabilities_payload();
CritiqueResponse critique_payload(const CritiqueRequest& request);
void register_routes(httplib::Server& server);

}  // namespace ppa::api
