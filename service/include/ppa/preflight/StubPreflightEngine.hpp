#pragma once

#include <string>

#include "ppa/core/engines.hpp"

namespace ppa {

class StubPreflightEngine final : public PreflightEngine {
public:
    PreflightReport run(const CritiqueRequest& request) override {
        const auto has_dimensions = request.metadata.width > 0 && request.metadata.height > 0;
        return PreflightReport{
            .status = has_dimensions ? "pass" : "warn",
            .checks = {PreflightCheck{.id = "dimensions",
                                      .result = has_dimensions ? "pass" : "warn",
                                      .message = has_dimensions ? "stub dimensions check completed"
                                                                : "dimensions missing from request"}},
            .technical_scores = TechnicalScores{},
        };
    }
};

}  // namespace ppa
