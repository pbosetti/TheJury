#pragma once

#include "ppa/core/engines.hpp"

namespace ppa {

class DisabledSemanticProvider final : public SemanticProvider {
public:
    SemanticResult evaluate(const CritiqueRequest&, const PreflightReport&) override {
        return SemanticResult{
            .summary = "semantic analysis disabled",
            .votes = {},
            .strengths = {},
            .improvements = {"Enable a semantic provider to receive narrative critique."},
        };
    }
};

}  // namespace ppa
