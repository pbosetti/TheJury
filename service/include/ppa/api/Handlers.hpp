#pragma once

#include <filesystem>

#include <httplib.h>

#include "ppa/core/CritiqueService.hpp"
#include "ppa/runtime/RuntimeManager.hpp"

namespace ppa::api {

nlohmann::json health_payload();
CapabilitiesResponse capabilities_payload(const CritiqueService& service);
CritiqueResponse critique_payload(const CritiqueService& service, const CritiqueRequest& request);
void register_routes(httplib::Server& server,
                     CritiqueService& service,
                     RuntimeManager& runtime_manager,
                     const std::filesystem::path& config_path);

}  // namespace ppa::api
