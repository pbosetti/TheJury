#include "ppa/api/Handlers.hpp"

#include <exception>
#include <iostream>

namespace ppa::api {
namespace {
constexpr auto* kJsonMimeType = "application/json";

CritiqueService& critique_service() {
    static CritiqueService service;
    return service;
}

void set_json(httplib::Response& response, const nlohmann::json& payload, int status = 200) {
    response.status = status;
    response.set_content(payload.dump(2), kJsonMimeType);
}
}  // namespace

nlohmann::json health_payload() {
    return nlohmann::json{{"status", "ok"}};
}

CapabilitiesResponse capabilities_payload() {
    return critique_service().capabilities();
}

CritiqueResponse critique_payload(const CritiqueRequest& request) {
    return critique_service().critique(request);
}

void register_routes(httplib::Server& server) {
    server.Get("/health", [](const httplib::Request&, httplib::Response& response) {
        set_json(response, health_payload());
    });

    server.Get("/v1/capabilities", [](const httplib::Request&, httplib::Response& response) {
        set_json(response, nlohmann::json(capabilities_payload()));
    });

    server.Post("/v1/critique", [](const httplib::Request& request, httplib::Response& response) {
        try {
            const auto payload = nlohmann::json::parse(request.body);
            const auto critique_request = payload.get<CritiqueRequest>();
            set_json(response, nlohmann::json(critique_payload(critique_request)));
        } catch (const std::exception& error) {
            std::clog << "critique request rejected: " << error.what() << '\n';
            set_json(response,
                     nlohmann::json{{"error", "invalid_request"}, {"message", error.what()}},
                     400);
        }
    });
}

}  // namespace ppa::api
