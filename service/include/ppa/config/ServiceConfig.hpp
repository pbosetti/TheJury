#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ppa {

struct JurorDefinition {
    std::string judge_id;
    std::string personality;
    double weight{1.0};
};

struct OllamaSettings {
    std::string base_url{"http://127.0.0.1:11434"};
    std::string model{"qwen2.5vl:7b"};
    std::string fallback_model{"qwen2.5vl:3b"};
    int timeout_ms{300000};
};

struct SemanticSettings {
    std::string default_provider{"ollama"};
};

struct ServiceConfig {
    OllamaSettings ollama;
    SemanticSettings semantic;
    std::vector<JurorDefinition> jurors;
};

struct LoadedServiceConfig {
    ServiceConfig config;
    std::filesystem::path resolved_path;
    bool from_file{false};
};

[[nodiscard]] ServiceConfig normalize_service_config(ServiceConfig config);
[[nodiscard]] LoadedServiceConfig load_service_config(const std::filesystem::path& executable_directory);
void write_service_config(const std::filesystem::path& output_path, const ServiceConfig& config);
[[nodiscard]] std::filesystem::path current_executable_path();
[[nodiscard]] std::vector<std::string> configured_models(const OllamaSettings& settings);

}  // namespace ppa
