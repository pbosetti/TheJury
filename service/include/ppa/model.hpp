#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ppa/config/ServiceConfig.hpp"

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
    std::vector<int> selected_jurors;
};

struct RequestMetadata {
    int width{0};
    int height{0};
    std::string icc_profile;
    std::vector<std::string> keywords;
    std::string original_path;
    std::string capture_time;
    std::string file_format;
    std::string color_label;
    int rating{0};
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

struct RuntimeLeaseRequest {
    std::string client;
    std::string instance_id;
    int ttl_seconds{45};
};

struct RuntimeLeaseResponse {
    std::string state{"stopped"};
    int expires_in_seconds{0};
    int active_lease_count{0};
};

struct RuntimeStatus {
    std::string state{"stopped"};
    bool reachable{false};
    std::string service{"ppa-companion"};
    std::string version{"0.1.0"};
    std::uint32_t pid{0};
    std::uint64_t uptime_seconds{0};
    int jobs_in_flight{0};
    int active_lease_count{0};
    int lease_ttl_seconds{45};
    std::string provider{"disabled"};
    std::string model;
    std::string last_error;
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

struct JudgeVote {
    std::string judge_id;
    std::string vote;
    double confidence{0.0};
    std::string rationale;
};

struct SemanticResult {
    std::string summary;
    std::vector<JudgeVote> votes;
    std::vector<std::string> strengths;
    std::vector<std::string> improvements;
};

struct SemanticOutput {
    SemanticResult result;
    std::string model;
};

struct AggregateResult {
    std::string classification{"C"};
    double merit_score{0.0};
    double merit_probability{0.0};
    double confidence{0.0};
    std::string summary{"stub critique response"};
};

struct CritiqueResponse {
    std::string request_id{"stub-0001"};
    RuntimeInfo runtime;
    PreflightReport preflight;
    std::optional<SemanticResult> semantic{std::nullopt};
    AggregateResult aggregate;
};

struct ErrorResponse {
    std::string error;
    std::string message;
};

inline void to_json(json& j, const JurorDefinition& value) {
    j = json{{"judge_id", value.judge_id},
             {"personality", value.personality},
             {"weight", value.weight}};
}
inline void from_json(const json& j, JurorDefinition& value) {
    if (j.contains("judge_id")) {
        j.at("judge_id").get_to(value.judge_id);
    }
    if (j.contains("personality")) {
        j.at("personality").get_to(value.personality);
    }
    if (j.contains("weight")) {
        j.at("weight").get_to(value.weight);
    }
}

inline void to_json(json& j, const OllamaSettings& value) {
    j = json{{"base_url", value.base_url},
             {"model", value.model},
             {"fallback_model", value.fallback_model},
             {"timeout_ms", value.timeout_ms}};
}
inline void from_json(const json& j, OllamaSettings& value) {
    if (j.contains("base_url")) {
        j.at("base_url").get_to(value.base_url);
    }
    if (j.contains("model")) {
        j.at("model").get_to(value.model);
    }
    if (j.contains("fallback_model")) {
        j.at("fallback_model").get_to(value.fallback_model);
    }
    if (j.contains("timeout_ms")) {
        j.at("timeout_ms").get_to(value.timeout_ms);
    }
}

inline void to_json(json& j, const SemanticSettings& value) {
    j = json{{"default_provider", value.default_provider}};
}
inline void from_json(const json& j, SemanticSettings& value) {
    if (j.contains("default_provider")) {
        j.at("default_provider").get_to(value.default_provider);
    }
}

inline void to_json(json& j, const ServiceConfig& value) {
    j = json{{"ollama", value.ollama}, {"semantic", value.semantic}, {"jurors", value.jurors}};
}
inline void from_json(const json& j, ServiceConfig& value) {
    if (j.contains("ollama")) {
        j.at("ollama").get_to(value.ollama);
    }
    if (j.contains("semantic")) {
        j.at("semantic").get_to(value.semantic);
    }
    if (j.contains("jurors")) {
        j.at("jurors").get_to(value.jurors);
    }
}

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
    j = json{{"run_preflight", value.run_preflight},
             {"run_semantic", value.run_semantic},
             {"semantic_provider", value.semantic_provider}};
    if (!value.selected_jurors.empty()) {
        j["selected_jurors"] = value.selected_jurors;
    }
}
inline void from_json(const json& j, CritiqueOptions& value) {
    j.at("run_preflight").get_to(value.run_preflight);
    j.at("run_semantic").get_to(value.run_semantic);
    j.at("semantic_provider").get_to(value.semantic_provider);
    if (j.contains("selected_jurors")) {
        j.at("selected_jurors").get_to(value.selected_jurors);
    }
}

inline void to_json(json& j, const RequestMetadata& value) {
    j = json{{"width", value.width},
             {"height", value.height},
             {"icc_profile", value.icc_profile},
             {"keywords", value.keywords},
             {"original_path", value.original_path},
             {"capture_time", value.capture_time},
             {"file_format", value.file_format},
             {"color_label", value.color_label},
             {"rating", value.rating}};
}
inline void from_json(const json& j, RequestMetadata& value) {
    j.at("width").get_to(value.width);
    j.at("height").get_to(value.height);
    j.at("icc_profile").get_to(value.icc_profile);
    j.at("keywords").get_to(value.keywords);
    if (j.contains("original_path")) {
        j.at("original_path").get_to(value.original_path);
    }
    if (j.contains("capture_time")) {
        j.at("capture_time").get_to(value.capture_time);
    }
    if (j.contains("file_format")) {
        j.at("file_format").get_to(value.file_format);
    }
    if (j.contains("color_label")) {
        j.at("color_label").get_to(value.color_label);
    }
    if (j.contains("rating")) {
        j.at("rating").get_to(value.rating);
    }
}

inline void to_json(json& j, const CritiqueRequest& value) {
    j = json{{"image", value.image},
             {"photo", value.photo},
             {"category", value.category},
             {"mode", value.mode},
             {"options", value.options},
             {"metadata", value.metadata}};
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
inline void from_json(const json& j, ProviderCapability& value) {
    j.at("name").get_to(value.name);
    j.at("available").get_to(value.available);
    if (j.contains("models")) {
        j.at("models").get_to(value.models);
    }
}

inline void to_json(json& j, const SemanticCapabilities& value) {
    j = json{{"enabled", value.enabled},
             {"default_provider", value.default_provider},
             {"providers", value.providers}};
}
inline void from_json(const json& j, SemanticCapabilities& value) {
    j.at("enabled").get_to(value.enabled);
    j.at("default_provider").get_to(value.default_provider);
    j.at("providers").get_to(value.providers);
}

inline void to_json(json& j, const CapabilitiesResponse& value) {
    j = json{{"service", value.service}, {"version", value.version}, {"semantic", value.semantic}};
}
inline void from_json(const json& j, CapabilitiesResponse& value) {
    j.at("service").get_to(value.service);
    j.at("version").get_to(value.version);
    j.at("semantic").get_to(value.semantic);
}

inline void to_json(json& j, const RuntimeInfo& value) {
    j = json{{"semantic_provider", value.semantic_provider}, {"model", value.model}};
}
inline void from_json(const json& j, RuntimeInfo& value) {
    j.at("semantic_provider").get_to(value.semantic_provider);
    j.at("model").get_to(value.model);
}

inline void to_json(json& j, const RuntimeLeaseRequest& value) {
    j = json{{"client", value.client},
             {"instance_id", value.instance_id},
             {"ttl_seconds", value.ttl_seconds}};
}
inline void from_json(const json& j, RuntimeLeaseRequest& value) {
    j.at("client").get_to(value.client);
    j.at("instance_id").get_to(value.instance_id);
    if (j.contains("ttl_seconds")) {
        j.at("ttl_seconds").get_to(value.ttl_seconds);
    }
}

inline void to_json(json& j, const RuntimeLeaseResponse& value) {
    j = json{{"state", value.state},
             {"expires_in_seconds", value.expires_in_seconds},
             {"active_lease_count", value.active_lease_count}};
}
inline void from_json(const json& j, RuntimeLeaseResponse& value) {
    j.at("state").get_to(value.state);
    j.at("expires_in_seconds").get_to(value.expires_in_seconds);
    j.at("active_lease_count").get_to(value.active_lease_count);
}

inline void to_json(json& j, const RuntimeStatus& value) {
    j = json{{"state", value.state},
             {"reachable", value.reachable},
             {"service", value.service},
             {"version", value.version},
             {"pid", value.pid},
             {"uptime_seconds", value.uptime_seconds},
             {"jobs_in_flight", value.jobs_in_flight},
             {"active_lease_count", value.active_lease_count},
             {"lease_ttl_seconds", value.lease_ttl_seconds},
             {"provider", value.provider},
             {"model", value.model},
             {"last_error", value.last_error}};
}
inline void from_json(const json& j, RuntimeStatus& value) {
    j.at("state").get_to(value.state);
    if (j.contains("reachable")) {
        j.at("reachable").get_to(value.reachable);
    }
    if (j.contains("service")) {
        j.at("service").get_to(value.service);
    }
    if (j.contains("version")) {
        j.at("version").get_to(value.version);
    }
    if (j.contains("pid")) {
        j.at("pid").get_to(value.pid);
    }
    if (j.contains("uptime_seconds")) {
        j.at("uptime_seconds").get_to(value.uptime_seconds);
    }
    if (j.contains("jobs_in_flight")) {
        j.at("jobs_in_flight").get_to(value.jobs_in_flight);
    }
    if (j.contains("active_lease_count")) {
        j.at("active_lease_count").get_to(value.active_lease_count);
    }
    if (j.contains("lease_ttl_seconds")) {
        j.at("lease_ttl_seconds").get_to(value.lease_ttl_seconds);
    }
    if (j.contains("provider")) {
        j.at("provider").get_to(value.provider);
    }
    if (j.contains("model")) {
        j.at("model").get_to(value.model);
    }
    if (j.contains("last_error")) {
        j.at("last_error").get_to(value.last_error);
    }
}

inline void to_json(json& j, const PreflightCheck& value) {
    j = json{{"id", value.id}, {"result", value.result}, {"message", value.message}};
}
inline void from_json(const json& j, PreflightCheck& value) {
    j.at("id").get_to(value.id);
    j.at("result").get_to(value.result);
    j.at("message").get_to(value.message);
}

inline void to_json(json& j, const TechnicalScores& value) {
    j = json{{"technical_excellence", value.technical_excellence},
             {"color_balance", value.color_balance},
             {"lighting", value.lighting},
             {"composition", value.composition}};
}
inline void from_json(const json& j, TechnicalScores& value) {
    j.at("technical_excellence").get_to(value.technical_excellence);
    j.at("color_balance").get_to(value.color_balance);
    j.at("lighting").get_to(value.lighting);
    j.at("composition").get_to(value.composition);
}

inline void to_json(json& j, const PreflightReport& value) {
    j = json{{"status", value.status}, {"checks", value.checks}, {"technical_scores", value.technical_scores}};
}
inline void from_json(const json& j, PreflightReport& value) {
    j.at("status").get_to(value.status);
    j.at("checks").get_to(value.checks);
    j.at("technical_scores").get_to(value.technical_scores);
}

inline void to_json(json& j, const JudgeVote& value) {
    j = json{{"judge_id", value.judge_id},
             {"vote", value.vote},
             {"confidence", value.confidence},
             {"rationale", value.rationale}};
}
inline void from_json(const json& j, JudgeVote& value) {
    j.at("judge_id").get_to(value.judge_id);
    j.at("vote").get_to(value.vote);
    j.at("confidence").get_to(value.confidence);
    j.at("rationale").get_to(value.rationale);
}

inline void to_json(json& j, const SemanticResult& value) {
    j = json{{"summary", value.summary},
             {"votes", value.votes},
             {"strengths", value.strengths},
             {"improvements", value.improvements}};
}
inline void from_json(const json& j, SemanticResult& value) {
    j.at("summary").get_to(value.summary);
    j.at("votes").get_to(value.votes);
    j.at("strengths").get_to(value.strengths);
    j.at("improvements").get_to(value.improvements);
}

inline void to_json(json& j, const ErrorResponse& value) {
    j = json{{"error", value.error}, {"message", value.message}};
}
inline void from_json(const json& j, ErrorResponse& value) {
    j.at("error").get_to(value.error);
    j.at("message").get_to(value.message);
}

inline void to_json(json& j, const AggregateResult& value) {
    j = json{{"classification", value.classification},
             {"merit_score", value.merit_score},
             {"merit_probability", value.merit_probability},
             {"confidence", value.confidence},
             {"summary", value.summary}};
}
inline void from_json(const json& j, AggregateResult& value) {
    j.at("classification").get_to(value.classification);
    if (j.contains("merit_score")) {
        j.at("merit_score").get_to(value.merit_score);
    }
    j.at("merit_probability").get_to(value.merit_probability);
    j.at("confidence").get_to(value.confidence);
    j.at("summary").get_to(value.summary);
}

inline void to_json(json& j, const CritiqueResponse& value) {
    j = json{{"request_id", value.request_id},
             {"runtime", value.runtime},
             {"preflight", value.preflight},
             {"semantic", value.semantic.has_value() ? json(*value.semantic) : json(nullptr)},
             {"aggregate", value.aggregate}};
}
inline void from_json(const json& j, CritiqueResponse& value) {
    j.at("request_id").get_to(value.request_id);
    j.at("runtime").get_to(value.runtime);
    j.at("preflight").get_to(value.preflight);
    if (j.contains("semantic") && !j.at("semantic").is_null()) {
        value.semantic = j.at("semantic").get<SemanticResult>();
    } else {
        value.semantic = std::nullopt;
    }
    j.at("aggregate").get_to(value.aggregate);
}

}  // namespace ppa
