#pragma once

#include <memory>
#include <utility>
#include <vector>

#include "ppa/core/engines.hpp"
#include "ppa/config/ServiceConfig.hpp"
#include "ppa/semantic/DisabledSemanticProvider.hpp"
#include "ppa/semantic/OllamaClient.hpp"
#include "ppa/semantic/OllamaProvider.hpp"

namespace ppa {

class SemanticProviderFactory {
public:
    explicit SemanticProviderFactory(OllamaClient client = OllamaClient(), std::vector<JurorDefinition> jurors = {})
        : _client(std::move(client)), _jurors(std::move(jurors)) {}

    [[nodiscard]] std::unique_ptr<SemanticProvider> create(const std::string& provider_name,
                                                           std::vector<JurorDefinition> jurors = {}) const {
        if (jurors.empty()) {
            jurors = _jurors;
        }
        if (provider_name == "ollama") {
            return std::make_unique<OllamaProvider>(_client, std::move(jurors));
        }
        return std::make_unique<DisabledSemanticProvider>();
    }

private:
    OllamaClient _client;
    std::vector<JurorDefinition> _jurors;
};

}  // namespace ppa
