#pragma once

#include <optional>

#include "ppa/core/engines.hpp"

namespace ppa {

class SimpleAggregateEngine final : public AggregateEngine {
public:
    AggregateResult combine(const PreflightReport& preflight,
                            const std::optional<SemanticResult>& semantic) override {
        AggregateResult result;
        result.classification = preflight.status == "pass" ? "C" : "D";
        result.merit_probability = semantic.has_value() ? 0.42 : 0.0;
        result.confidence = semantic.has_value() ? 0.35 : 0.15;
        result.summary = semantic.has_value() ? semantic->summary : "stub critique response";
        return result;
    }
};

}  // namespace ppa
