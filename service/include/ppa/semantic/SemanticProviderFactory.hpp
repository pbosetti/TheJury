#pragma once

#include <memory>
#include <utility>

#include "ppa/core/engines.hpp"
#include "ppa/semantic/DisabledSemanticProvider.hpp"
#include "ppa/semantic/OllamaClient.hpp"
#include "ppa/semantic/OllamaProvider.hpp"

namespace ppa {

class SemanticProviderFactory {
public:
    explicit SemanticProviderFactory(OllamaClient client = OllamaClient{}) : _client(std::move(client)) {}

    [[nodiscard]] std::unique_ptr<SemanticProvider> create(const std::string& provider_name) const {
        if (provider_name == "ollama") {
            return std::make_unique<OllamaProvider>(_client);
        }
        return std::make_unique<DisabledSemanticProvider>();
    }

private:
    OllamaClient _client;
};

}  // namespace ppa
