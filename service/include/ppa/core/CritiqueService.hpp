#pragma once

#include <optional>
#include <string>

#include "ppa/aggregate/SimpleAggregateEngine.hpp"
#include "ppa/preflight/StubPreflightEngine.hpp"
#include "ppa/semantic/OllamaClient.hpp"
#include "ppa/semantic/SemanticProviderFactory.hpp"

namespace ppa {

class CritiqueService {
public:
    CritiqueService() = default;

    [[nodiscard]] CapabilitiesResponse capabilities() const {
        return CapabilitiesResponse{
            .service = "ppa-companion",
            .version = "0.1.0",
            .semantic = SemanticCapabilities{
                .enabled = true,
                .default_provider = "ollama",
                .providers = {
                    ProviderCapability{.name = "disabled", .available = true, .models = {}},
                    ProviderCapability{.name = "ollama",
                                       .available = ollama_client_.is_available(),
                                       .models = ollama_client_.configured_models()},
                },
            },
        };
    }

    [[nodiscard]] CritiqueResponse critique(const CritiqueRequest& request) const {
        auto preflight = preflight_engine_.run(request);
        if (!request.options.run_preflight) {
            preflight.status = "skipped";
            preflight.checks = {PreflightCheck{.id = "preflight", .result = "skipped", .message = "preflight disabled by request"}};
        }

        std::optional<SemanticResult> semantic = std::nullopt;
        auto provider_name = std::string{"disabled"};
        auto model = std::string{};
        if (request.options.run_semantic) {
            provider_name = request.options.semantic_provider.empty() ? "disabled" : request.options.semantic_provider;
            auto provider = semantic_factory_.create(provider_name);
            semantic = provider->evaluate(request, preflight);
            if (provider_name == "ollama") {
                model = ollama_client_.default_model();
            }
        }

        return CritiqueResponse{
            .request_id = "stub-0001",
            .runtime = RuntimeInfo{.semantic_provider = provider_name, .model = model},
            .preflight = preflight,
            .semantic = semantic,
            .aggregate = aggregate_engine_.combine(preflight, semantic),
        };
    }

private:
    mutable StubPreflightEngine preflight_engine_;
    mutable SimpleAggregateEngine aggregate_engine_;
    OllamaClient ollama_client_;
    SemanticProviderFactory semantic_factory_{ollama_client_};
};

}  // namespace ppa
