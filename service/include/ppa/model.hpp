#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ppa {

using json = nlohmann::json;

struct ImageInput {
    std::string path;
};

struct PhotoInfo {
    std::string id;
    std::string file_name;
};

struct CritiqueOptions {
    bool run_preflight{true};
    bool run_semantic{false};
    std::string semantic_provider{"disabled"};
};

struct RequestMetadata {
    int width{0};
    int height{0};
    std::string icc_profile;
    std::vector<std::string> keywords;
};

struct CritiqueRequest {
    ImageInput image;
    PhotoInfo photo;
    std::string category;
    std::string mode;
    CritiqueOptions options;
    RequestMetadata metadata;
};

struct ProviderCapability {
    std::string name;
    bool available{false};
    std::vector<std::string> models;
};

struct SemanticCapabilities {
    bool enabled{true};
    std::string default_provider{"ollama"};
    std::vector<ProviderCapability> providers;
};

struct CapabilitiesResponse {
    std::string service{"ppa-companion"};
    std::string version{"0.1.0"};
    SemanticCapabilities semantic;
};

struct RuntimeInfo {
    std::string semantic_provider{"disabled"};
    std::string model;
};

struct PreflightCheck {
    std::string id;
    std::string result;
    std::string message;
};

struct TechnicalScores {
    double technical_excellence{0.0};
    double color_balance{0.0};
    double lighting{0.0};
    double composition{0.0};
};

struct PreflightReport {
    std::string status{"pass"};
    std::vector<PreflightCheck> checks;
    TechnicalScores technical_scores;
};

struct AggregateResult {
    std::string classification{"C"};
    double merit_probability{0.0};
    double confidence{0.0};
    std::string summary{"stub critique response"};
};

struct CritiqueResponse {
    std::string request_id{"stub-0001"};
    RuntimeInfo runtime;
    PreflightReport preflight;
    std::optional<json> semantic{std::nullopt};
    AggregateResult aggregate;
};

inline void to_json(json& j, const ImageInput& value) {
    j = json{{"path", value.path}};
}
inline void from_json(const json& j, ImageInput& value) {
    j.at("path").get_to(value.path);
}

inline void to_json(json& j, const PhotoInfo& value) {
    j = json{{"id", value.id}, {"file_name", value.file_name}};
}
inline void from_json(const json& j, PhotoInfo& value) {
    j.at("id").get_to(value.id);
    j.at("file_name").get_to(value.file_name);
}

inline void to_json(json& j, const CritiqueOptions& value) {
    j = json{{"run_preflight", value.run_preflight}, {"run_semantic", value.run_semantic}, {"semantic_provider", value.semantic_provider}};
}
inline void from_json(const json& j, CritiqueOptions& value) {
    j.at("run_preflight").get_to(value.run_preflight);
    j.at("run_semantic").get_to(value.run_semantic);
    j.at("semantic_provider").get_to(value.semantic_provider);
}

inline void to_json(json& j, const RequestMetadata& value) {
    j = json{{"width", value.width}, {"height", value.height}, {"icc_profile", value.icc_profile}, {"keywords", value.keywords}};
}
inline void from_json(const json& j, RequestMetadata& value) {
    j.at("width").get_to(value.width);
    j.at("height").get_to(value.height);
    j.at("icc_profile").get_to(value.icc_profile);
    j.at("keywords").get_to(value.keywords);
}

inline void to_json(json& j, const CritiqueRequest& value) {
    j = json{{"image", value.image}, {"photo", value.photo}, {"category", value.category}, {"mode", value.mode}, {"options", value.options}, {"metadata", value.metadata}};
}
inline void from_json(const json& j, CritiqueRequest& value) {
    j.at("image").get_to(value.image);
    j.at("photo").get_to(value.photo);
    j.at("category").get_to(value.category);
    j.at("mode").get_to(value.mode);
    j.at("options").get_to(value.options);
    j.at("metadata").get_to(value.metadata);
}

inline void to_json(json& j, const ProviderCapability& value) {
    j = json{{"name", value.name}, {"available", value.available}};
    if (!value.models.empty()) {
        j["models"] = value.models;
    }
}

inline void to_json(json& j, const SemanticCapabilities& value) {
    j = json{{"enabled", value.enabled}, {"default_provider", value.default_provider}, {"providers", value.providers}};
}

inline void to_json(json& j, const CapabilitiesResponse& value) {
    j = json{{"service", value.service}, {"version", value.version}, {"semantic", value.semantic}};
}

inline void to_json(json& j, const RuntimeInfo& value) {
    j = json{{"semantic_provider", value.semantic_provider}, {"model", value.model}};
}

inline void to_json(json& j, const PreflightCheck& value) {
    j = json{{"id", value.id}, {"result", value.result}, {"message", value.message}};
}

inline void to_json(json& j, const TechnicalScores& value) {
    j = json{{"technical_excellence", value.technical_excellence}, {"color_balance", value.color_balance}, {"lighting", value.lighting}, {"composition", value.composition}};
}

inline void to_json(json& j, const PreflightReport& value) {
    j = json{{"status", value.status}, {"checks", value.checks}, {"technical_scores", value.technical_scores}};
}

inline void to_json(json& j, const AggregateResult& value) {
    j = json{{"classification", value.classification}, {"merit_probability", value.merit_probability}, {"confidence", value.confidence}, {"summary", value.summary}};
}

inline void to_json(json& j, const CritiqueResponse& value) {
    j = json{{"request_id", value.request_id}, {"runtime", value.runtime}, {"preflight", value.preflight}, {"semantic", value.semantic.has_value() ? *value.semantic : json(nullptr)}, {"aggregate", value.aggregate}};
}

}  // namespace ppa
