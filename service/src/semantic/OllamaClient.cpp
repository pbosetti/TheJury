#include "ppa/semantic/OllamaClient.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <httplib.h>

#include "ppa/api/ApiError.hpp"
#include "ppa/config/ServiceConfig.hpp"

namespace ppa {
namespace {

class HttplibOllamaTransport final : public OllamaTransport {
public:
    explicit HttplibOllamaTransport(OllamaSettings settings) : _settings(std::move(settings)) {}

    [[nodiscard]] OllamaHttpResponse get(const std::string& path) const override {
        auto client = httplib::Client(_settings.base_url);
        client.set_connection_timeout(std::chrono::milliseconds(_settings.timeout_ms));
        client.set_read_timeout(std::chrono::milliseconds(_settings.timeout_ms));
        client.set_write_timeout(std::chrono::milliseconds(_settings.timeout_ms));

        if (const auto response = client.Get(path)) {
            return OllamaHttpResponse{.status = response->status, .body = response->body, .error = ""};
        }

        return OllamaHttpResponse{.status = 0, .body = "", .error = "request to Ollama failed"};
    }

    [[nodiscard]] OllamaHttpResponse post(const std::string& path, const std::string& body) const override {
        auto client = httplib::Client(_settings.base_url);
        client.set_connection_timeout(std::chrono::milliseconds(_settings.timeout_ms));
        client.set_read_timeout(std::chrono::milliseconds(_settings.timeout_ms));
        client.set_write_timeout(std::chrono::milliseconds(_settings.timeout_ms));

        if (const auto response = client.Post(path, body, "application/json")) {
            return OllamaHttpResponse{.status = response->status, .body = response->body, .error = ""};
        }

        return OllamaHttpResponse{.status = 0, .body = "", .error = "request to Ollama failed"};
    }

private:
    OllamaSettings _settings;
};

std::string read_file_bytes(const std::string& path) {
    auto input = std::ifstream(path, std::ios::binary);
    if (!input) {
        throw api::ApiError(400, "invalid_request", "image file not found: " + path);
    }

    auto stream = std::ostringstream{};
    stream << input.rdbuf();
    const auto content = stream.str();
    if (content.empty()) {
        throw api::ApiError(400, "invalid_request", "image file is empty: " + path);
    }
    return content;
}

std::string base64_encode(const std::string& input) {
    static constexpr char kTable[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    int value = 0;
    int bits = -6;
    for (const auto character : input) {
        value = (value << 8) + static_cast<unsigned char>(character);
        bits += 8;
        while (bits >= 0) {
            output.push_back(kTable[(value >> bits) & 0x3F]);
            bits -= 6;
        }
    }

    if (bits > -6) {
        output.push_back(kTable[((value << 8) >> (bits + 8)) & 0x3F]);
    }
    while (output.size() % 4 != 0) {
        output.push_back('=');
    }

    return output;
}

nlohmann::json output_schema() {
    return nlohmann::json{
        {"type", "object"},
        {"properties",
         {
             {"summary", {{"type", "string"}}},
             {"votes",
              {{"type", "array"},
               {"minItems", 1},
               {"items",
                {{"type", "object"},
                 {"properties",
                  {{"judge_id", {{"type", "string"}}},
                   {"vote", {{"type", "string"}}},
                   {"confidence", {{"type", "number"}}},
                   {"rationale", {{"type", "string"}}}}},
                 {"required", {"judge_id", "vote", "confidence", "rationale"}}}}}},
             {"strengths", {{"type", "array"}, {"items", {{"type", "string"}}}}},
             {"improvements", {{"type", "array"}, {"items", {{"type", "string"}}}}},
         }},
        {"required", {"summary", "votes", "strengths", "improvements"}},
    };
}

std::string extract_error_message(const std::string& body) {
    if (body.empty()) {
        return {};
    }

    try {
        const auto payload = nlohmann::json::parse(body);
        if (payload.contains("error") && payload.at("error").is_string()) {
            return payload.at("error").get<std::string>();
        }
    } catch (const std::exception&) {
    }

    return body;
}

std::vector<std::string> installed_models(const OllamaTransport& transport) {
    const auto response = transport.get("/api/tags");
    if (!response.error.empty() || response.status != 200) {
        return {};
    }

    try {
        const auto payload = nlohmann::json::parse(response.body);
        if (!payload.contains("models") || !payload.at("models").is_array()) {
            return {};
        }

        auto models = std::vector<std::string>{};
        for (const auto& model : payload.at("models")) {
            if (model.contains("name") && model.at("name").is_string()) {
                models.push_back(model.at("name").get<std::string>());
            } else if (model.contains("model") && model.at("model").is_string()) {
                models.push_back(model.at("model").get<std::string>());
            }
        }
        return models;
    } catch (const std::exception&) {
        return {};
    }
}

SemanticResult parse_semantic_result(const std::string& body) {
    nlohmann::json response_json;
    try {
        response_json = nlohmann::json::parse(body);
    } catch (const std::exception& error) {
        throw api::ApiError(502, "semantic_invalid_response", "Ollama returned invalid JSON: " + std::string(error.what()));
    }

    if (!response_json.contains("message") || !response_json.at("message").is_object() ||
        !response_json.at("message").contains("content") || !response_json.at("message").at("content").is_string()) {
        throw api::ApiError(502, "semantic_invalid_response", "Ollama response did not contain message.content");
    }

    try {
        auto semantic_json = nlohmann::json::parse(response_json.at("message").at("content").get<std::string>());
        auto semantic = semantic_json.get<SemanticResult>();
        if (semantic.votes.empty()) {
            throw api::ApiError(502, "semantic_invalid_response", "Ollama response must contain at least one judge vote");
        }
        for (const auto& vote : semantic.votes) {
            if (vote.vote != "C" && vote.vote != "D") {
                throw api::ApiError(502, "semantic_invalid_response", "Ollama vote must be C or D");
            }
        }
        return semantic;
    } catch (const api::ApiError&) {
        throw;
    } catch (const std::exception& error) {
        throw api::ApiError(502,
                            "semantic_invalid_response",
                            "Failed to parse structured semantic output: " + std::string(error.what()));
    }
}

}  // namespace

OllamaClient::OllamaClient(OllamaSettings settings)
    : OllamaClient(settings, std::make_shared<HttplibOllamaTransport>(settings)) {}

OllamaClient::OllamaClient(OllamaSettings settings, std::shared_ptr<OllamaTransport> transport)
    : _settings(std::move(settings)), _transport(std::move(transport)) {}

std::vector<std::string> OllamaClient::configured_models() const {
    return ppa::configured_models(_settings);
}

std::vector<std::string> OllamaClient::available_models() const {
    return installed_models(*_transport);
}

std::string OllamaClient::default_model() const {
    return configured_models().front();
}

bool OllamaClient::is_available() const {
    const auto response = _transport->get("/api/tags");
    return response.error.empty() && response.status == 200;
}

SemanticOutput OllamaClient::evaluate(const std::string& prompt, const std::string& image_path) const {
    const auto encoded_image = base64_encode(read_file_bytes(image_path));
    const auto models = configured_models();
    const auto installed = installed_models(*_transport);

    auto last_error = std::string{};
    auto last_status = 0;
    auto missing_models = std::vector<std::string>{};

    for (const auto& model : models) {
        const auto payload = nlohmann::json{
            {"model", model},
            {"stream", false},
            {"format", output_schema()},
            {"messages",
             nlohmann::json::array({
                 nlohmann::json{
                     {"role", "user"},
                     {"content", prompt},
                     {"images", nlohmann::json::array({encoded_image})},
                 },
             })},
            {"options", {{"temperature", 0}}},
        };

        const auto response = _transport->post("/api/chat", payload.dump());
        last_status = response.status;
        if (!response.error.empty()) {
            last_error = response.error;
            continue;
        }

        if (response.status != 200) {
            const auto response_error = extract_error_message(response.body);
            last_error = "Ollama returned HTTP " + std::to_string(response.status) +
                         (response_error.empty() ? std::string{} : ": " + response_error);
            if (response.status == 404) {
                missing_models.push_back(model);
            }
            continue;
        }

        return SemanticOutput{
            .result = parse_semantic_result(response.body),
            .model = model,
        };
    }

    if (last_status == 0) {
        throw api::ApiError(503,
                            "semantic_unavailable",
                            "Ollama is not reachable at " + _settings.base_url +
                                (last_error.empty() ? std::string{} : " (" + last_error + ")"));
    }

    if (!missing_models.empty()) {
        auto message = std::string{"Configured Ollama models are not installed: "};
        for (std::size_t index = 0; index < missing_models.size(); ++index) {
            if (index != 0) {
                message += ", ";
            }
            message += missing_models[index];
        }
        if (!installed.empty()) {
            message += ". Installed models: ";
            for (std::size_t index = 0; index < installed.size(); ++index) {
                if (index != 0) {
                    message += ", ";
                }
                message += installed[index];
            }
        }
        message += ". Update ppa_service.toml or run `ollama pull` for the configured model.";
        throw api::ApiError(502, "semantic_model_missing", message);
    }

    throw api::ApiError(502,
                        "semantic_failed",
                        "Ollama semantic request failed for all configured models" +
                            (last_error.empty() ? std::string{} : ": " + last_error));
}

}  // namespace ppa
