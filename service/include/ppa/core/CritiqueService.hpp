#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "ppa/aggregate/SimpleAggregateEngine.hpp"
#include "ppa/api/ApiError.hpp"
#include "ppa/config/ServiceConfig.hpp"
#include "ppa/preflight/StubPreflightEngine.hpp"
#include "ppa/runtime/RuntimeSupport.hpp"
#include "ppa/semantic/OllamaClient.hpp"
#include "ppa/semantic/SemanticProviderFactory.hpp"

namespace ppa {

class CritiqueService {
public:
    CritiqueService() : CritiqueService(ServiceConfig{}) {}
    explicit CritiqueService(ServiceConfig config)
        : CritiqueService(config, OllamaClient{config.ollama}) {}
    CritiqueService(ServiceConfig config, OllamaClient client)
        : _config(normalize_service_config(std::move(config))),
          _aggregate_engine(_config.jurors),
          _ollama_client(std::move(client)),
          _semantic_factory(_ollama_client, _config.jurors) {}

    [[nodiscard]] const ServiceConfig& config() const { return _config; }

    void update_config(ServiceConfig config) {
        _config = normalize_service_config(std::move(config));
        _aggregate_engine = SimpleAggregateEngine{_config.jurors};
        _ollama_client = OllamaClient{_config.ollama};
        _semantic_factory = SemanticProviderFactory{_ollama_client, _config.jurors};
    }

    [[nodiscard]] std::vector<std::string> available_models() const { return _ollama_client.available_models(); }

    [[nodiscard]] CapabilitiesResponse capabilities() const {
        return CapabilitiesResponse{
            .service = runtime::kServiceName,
            .version = runtime::kServiceVersion,
            .semantic = SemanticCapabilities{
                .enabled = true,
                .default_provider = _config.semantic.default_provider,
                .providers = {
                    ProviderCapability{.name = "disabled", .available = true, .models = {}},
                    ProviderCapability{.name = "ollama",
                                       .available = _ollama_client.is_available(),
                                       .models = _ollama_client.configured_models()},
                },
            },
        };
    }

    [[nodiscard]] CritiqueResponse critique(const CritiqueRequest& request) const {
        auto preflight = _preflight_engine.run(request);
        if (!request.options.run_preflight) {
            preflight.status = "skipped";
            preflight.checks = {PreflightCheck{.id = "preflight", .result = "skipped", .message = "preflight disabled by request"}};
        }

        const auto active_jurors = resolve_active_jurors(request.options.selected_jurors);
        std::optional<SemanticResult> semantic = std::nullopt;
        auto provider_name = std::string{"disabled"};
        auto model = std::string{};
        if (request.options.run_semantic) {
            provider_name = request.options.semantic_provider.empty() ? _config.semantic.default_provider
                                                                      : request.options.semantic_provider;
            auto provider = _semantic_factory.create(provider_name, active_jurors);
            const auto semantic_output = provider->evaluate(request, preflight);
            semantic = semantic_output.result;
            model = semantic_output.model;
        }

        auto aggregate_engine = SimpleAggregateEngine{active_jurors};
        return CritiqueResponse{
            .request_id = "stub-0001",
            .runtime = RuntimeInfo{.semantic_provider = provider_name, .model = model},
            .preflight = preflight,
            .semantic = semantic,
            .aggregate = aggregate_engine.combine(preflight, semantic),
        };
    }

private:
    [[nodiscard]] std::vector<JurorDefinition> resolve_active_jurors(const std::vector<int>& selected_indices) const {
        if (selected_indices.empty()) {
            return _config.jurors;
        }

        auto active_jurors = std::vector<JurorDefinition>{};
        active_jurors.reserve(selected_indices.size());
        auto seen = std::unordered_set<int>{};

        for (const auto index : selected_indices) {
            if (index <= 0 || static_cast<std::size_t>(index) > _config.jurors.size()) {
                throw api::ApiError(400,
                                    "invalid_request",
                                    "selected_jurors contains out-of-range juror index " + std::to_string(index));
            }
            if (!seen.insert(index).second) {
                continue;
            }
            active_jurors.push_back(_config.jurors[static_cast<std::size_t>(index - 1)]);
        }

        if (active_jurors.empty()) {
            throw api::ApiError(400, "invalid_request", "selected_jurors must not resolve to an empty juror panel");
        }

        return active_jurors;
    }

    ServiceConfig _config;
    mutable StubPreflightEngine _preflight_engine;
    mutable SimpleAggregateEngine _aggregate_engine;
    OllamaClient _ollama_client;
    SemanticProviderFactory _semantic_factory;
};

}  // namespace ppa
