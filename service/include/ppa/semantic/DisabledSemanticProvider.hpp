#pragma once

#include "ppa/core/engines.hpp"

namespace ppa {

class DisabledSemanticProvider final : public SemanticProvider {
public:
    SemanticOutput evaluate(const CritiqueRequest&, const PreflightReport&) override {
        return SemanticOutput{
            .result =
                SemanticResult{
                    .summary = "semantic analysis disabled",
                    .votes = {},
                    .strengths = {},
                    .improvements = {"Enable a semantic provider to receive narrative critique."},
                },
            .model = "",
        };
    }
};

}  // namespace ppa
