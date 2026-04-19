#pragma once

#include <optional>
#include <string>

#include "ppa/aggregate/SimpleAggregateEngine.hpp"
#include "ppa/config/ServiceConfig.hpp"
#include "ppa/preflight/StubPreflightEngine.hpp"
#include "ppa/semantic/OllamaClient.hpp"
#include "ppa/semantic/SemanticProviderFactory.hpp"

namespace ppa {

class CritiqueService {
public:
    CritiqueService() : CritiqueService(ServiceConfig{}) {}
    explicit CritiqueService(ServiceConfig config)
        : CritiqueService(config, OllamaClient{config.ollama}) {}
    CritiqueService(ServiceConfig config, OllamaClient client)
        : _config(std::move(config)),
          _ollama_client(std::move(client)),
          _semantic_factory(_ollama_client) {}

    [[nodiscard]] CapabilitiesResponse capabilities() const {
        return CapabilitiesResponse{
            .service = "ppa-companion",
            .version = "0.1.0",
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

        std::optional<SemanticResult> semantic = std::nullopt;
        auto provider_name = std::string{"disabled"};
        auto model = std::string{};
        if (request.options.run_semantic) {
            provider_name = request.options.semantic_provider.empty() ? _config.semantic.default_provider
                                                                      : request.options.semantic_provider;
            auto provider = _semantic_factory.create(provider_name);
            const auto semantic_output = provider->evaluate(request, preflight);
            semantic = semantic_output.result;
            model = semantic_output.model;
        }

        return CritiqueResponse{
            .request_id = "stub-0001",
            .runtime = RuntimeInfo{.semantic_provider = provider_name, .model = model},
            .preflight = preflight,
            .semantic = semantic,
            .aggregate = _aggregate_engine.combine(preflight, semantic),
        };
    }

private:
    ServiceConfig _config;
    mutable StubPreflightEngine _preflight_engine;
    mutable SimpleAggregateEngine _aggregate_engine;
    OllamaClient _ollama_client;
    SemanticProviderFactory _semantic_factory;
};

}  // namespace ppa
