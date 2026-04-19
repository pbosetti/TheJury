#include "ppa/config/ServiceConfig.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <toml++/toml.hpp>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace ppa {
namespace {

std::string require_non_empty(std::string value, const std::string& field_name) {
    if (value.empty()) {
        throw std::runtime_error("invalid configuration: " + field_name + " must not be empty");
    }
    return value;
}

void validate(ServiceConfig& config) {
    config.ollama.base_url = require_non_empty(std::move(config.ollama.base_url), "ollama.base_url");
    config.ollama.model = require_non_empty(std::move(config.ollama.model), "ollama.model");
    config.semantic.default_provider =
        require_non_empty(std::move(config.semantic.default_provider), "semantic.default_provider");

    if (config.ollama.timeout_ms <= 0) {
        throw std::runtime_error("invalid configuration: ollama.timeout_ms must be greater than zero");
    }

    if (config.ollama.fallback_model.empty()) {
        config.ollama.fallback_model = config.ollama.model;
    }
}

#if !defined(NDEBUG)
void log_loaded_config(const LoadedServiceConfig& loaded) {
    std::clog << "[debug] service config path: " << loaded.resolved_path.string()
              << (loaded.from_file ? " (loaded)" : " (defaults)") << '\n';
    std::clog << "[debug] semantic.default_provider: " << loaded.config.semantic.default_provider << '\n';
    std::clog << "[debug] ollama.base_url: " << loaded.config.ollama.base_url << '\n';
    std::clog << "[debug] ollama.model: " << loaded.config.ollama.model << '\n';
    std::clog << "[debug] ollama.fallback_model: " << loaded.config.ollama.fallback_model << '\n';
    std::clog << "[debug] ollama.timeout_ms: " << loaded.config.ollama.timeout_ms << '\n';
}
#else
void log_loaded_config(const LoadedServiceConfig&) {}
#endif

}  // namespace

std::filesystem::path current_executable_path() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    auto size = static_cast<DWORD>(buffer.size());
    auto written = GetModuleFileNameW(nullptr, buffer.data(), size);
    if (written == 0) {
        throw std::runtime_error("failed to resolve executable path");
    }
    buffer.resize(written);
    return std::filesystem::path(buffer);
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        throw std::runtime_error("failed to resolve executable path");
    }
    return std::filesystem::weakly_canonical(std::filesystem::path(buffer.c_str()));
#elif defined(__linux__)
    std::string buffer(4096, '\0');
    const auto written = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (written <= 0) {
        throw std::runtime_error("failed to resolve executable path");
    }
    buffer.resize(static_cast<std::size_t>(written));
    return std::filesystem::path(buffer);
#else
    throw std::runtime_error("current_executable_path is not implemented on this platform");
#endif
}

LoadedServiceConfig load_service_config(const std::filesystem::path& executable_directory) {
    LoadedServiceConfig loaded;
    loaded.resolved_path = executable_directory / "ppa_service.toml";

    if (std::filesystem::exists(loaded.resolved_path)) {
        try {
            const auto table = toml::parse_file(loaded.resolved_path.string());

            if (const auto base_url = table["ollama"]["base_url"].value<std::string>()) {
                loaded.config.ollama.base_url = *base_url;
            }
            if (const auto model = table["ollama"]["model"].value<std::string>()) {
                loaded.config.ollama.model = *model;
            }
            if (const auto fallback_model = table["ollama"]["fallback_model"].value<std::string>()) {
                loaded.config.ollama.fallback_model = *fallback_model;
            }
            if (const auto timeout_ms = table["ollama"]["timeout_ms"].value<int64_t>()) {
                loaded.config.ollama.timeout_ms = static_cast<int>(*timeout_ms);
            }
            if (const auto default_provider = table["semantic"]["default_provider"].value<std::string>()) {
                loaded.config.semantic.default_provider = *default_provider;
            }
            loaded.from_file = true;
        } catch (const toml::parse_error& error) {
            throw std::runtime_error(
                "failed to parse service config " + loaded.resolved_path.string() + ": " +
                std::string(error.description()));
        }
    }

    loaded.config = normalize_service_config(std::move(loaded.config));
    log_loaded_config(loaded);
    return loaded;
}

ServiceConfig normalize_service_config(ServiceConfig config) {
    validate(config);
    return config;
}

void write_service_config(const std::filesystem::path& output_path, const ServiceConfig& input_config) {
    const auto config = normalize_service_config(input_config);

    auto table = toml::table{};
    table.insert_or_assign("ollama",
                           toml::table{
                               {"base_url", config.ollama.base_url},
                               {"model", config.ollama.model},
                               {"fallback_model", config.ollama.fallback_model},
                               {"timeout_ms", config.ollama.timeout_ms},
                           });
    table.insert_or_assign("semantic",
                           toml::table{
                               {"default_provider", config.semantic.default_provider},
                           });

    auto output = std::ofstream(output_path);
    if (!output) {
        throw std::runtime_error("failed to open service config for writing: " + output_path.string());
    }

    output << table;
    if (!output) {
        throw std::runtime_error("failed to write service config: " + output_path.string());
    }
}

std::vector<std::string> configured_models(const OllamaSettings& settings) {
    std::vector<std::string> models;
    models.push_back(settings.model);
    if (settings.fallback_model != settings.model) {
        models.push_back(settings.fallback_model);
    }
    models.erase(std::remove(models.begin(), models.end(), std::string{}), models.end());
    return models;
}

}  // namespace ppa
