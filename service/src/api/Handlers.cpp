#include "ppa/api/Handlers.hpp"

#include <exception>
#include <iostream>

#include "ppa/api/ApiError.hpp"

namespace ppa::api {
namespace {
constexpr auto* kJsonMimeType = "application/json";

void set_json(httplib::Response& response, const nlohmann::json& payload, int status = 200) {
    response.status = status;
    response.set_content(payload.dump(2), kJsonMimeType);
}

#if !defined(NDEBUG)
void log_critique_request(const CritiqueRequest& request, const nlohmann::json& payload) {
    std::clog << "[debug] POST /v1/critique received for " << request.photo.file_name << '\n';
    std::clog << "[debug] image path: " << request.image.path << '\n';
    std::clog << "[debug] original path: " << request.metadata.original_path << '\n';
    std::clog << "[debug] dimensions: " << request.metadata.width << 'x' << request.metadata.height << '\n';
    std::clog << "[debug] format/profile: " << request.metadata.file_format << " / " << request.metadata.icc_profile
              << '\n';
    std::clog << "[debug] capture time: " << request.metadata.capture_time << '\n';
    std::clog << "[debug] color label/rating: " << request.metadata.color_label << " / " << request.metadata.rating
              << '\n';
    std::clog << "[debug] keywords: " << request.metadata.keywords.size() << '\n';
    std::clog << payload.dump(2) << '\n';
}

void log_critique_response(const CritiqueResponse& response) {
    std::clog << "[debug] critique response classification: " << response.aggregate.classification << '\n';
    std::clog << "[debug] critique response summary: " << response.aggregate.summary << '\n';
}
#else
void log_critique_request(const CritiqueRequest&, const nlohmann::json&) {}
void log_critique_response(const CritiqueResponse&) {}
#endif

nlohmann::json config_payload(const CritiqueService& service,
                             const std::filesystem::path& config_path) {
    return nlohmann::json{
        {"ollama", service.config().ollama},
        {"semantic", service.config().semantic},
        {"available_models", service.available_models()},
        {"path", config_path.string()},
        {"from_file", std::filesystem::exists(config_path)},
    };
}
}  // namespace

nlohmann::json health_payload() {
    return nlohmann::json{{"status", "ok"}};
}

CapabilitiesResponse capabilities_payload(const CritiqueService& service) {
    return service.capabilities();
}

CritiqueResponse critique_payload(const CritiqueService& service, const CritiqueRequest& request) {
    return service.critique(request);
}

void register_routes(httplib::Server& server, CritiqueService& service, const std::filesystem::path& config_path) {
    server.Get("/health", [](const httplib::Request&, httplib::Response& response) {
        set_json(response, health_payload());
    });

    server.Get("/v1/capabilities", [&service](const httplib::Request&, httplib::Response& response) {
        set_json(response, nlohmann::json(capabilities_payload(service)));
    });

    server.Get("/v1/config", [&service, &config_path](const httplib::Request&, httplib::Response& response) {
        set_json(response, config_payload(service, config_path));
    });

    server.Put("/v1/config", [&service, &config_path](const httplib::Request& request, httplib::Response& response) {
        try {
            auto config = nlohmann::json::parse(request.body).get<ServiceConfig>();
            config = normalize_service_config(std::move(config));
            write_service_config(config_path, config);
            service.update_config(config);
            set_json(response, config_payload(service, config_path));
        } catch (const ApiError& error) {
            set_json(response, nlohmann::json(ErrorResponse{.error = error.code(), .message = error.what()}), error.status());
        } catch (const std::exception& error) {
            set_json(response, nlohmann::json(ErrorResponse{.error = "invalid_config", .message = error.what()}), 400);
        }
    });

    server.Post("/v1/critique", [&service](const httplib::Request& request, httplib::Response& response) {
        try {
            const auto payload = nlohmann::json::parse(request.body);
            const auto critique_request = payload.get<CritiqueRequest>();
            log_critique_request(critique_request, payload);
            const auto critique_response = critique_payload(service, critique_request);
            log_critique_response(critique_response);
            set_json(response, nlohmann::json(critique_response));
        } catch (const ApiError& error) {
            set_json(response, nlohmann::json(ErrorResponse{.error = error.code(), .message = error.what()}), error.status());
        } catch (const std::exception& error) {
            std::clog << "critique request rejected: " << error.what() << '\n';
            set_json(response, nlohmann::json(ErrorResponse{.error = "invalid_request", .message = error.what()}), 400);
        }
    });
}

}  // namespace ppa::api
