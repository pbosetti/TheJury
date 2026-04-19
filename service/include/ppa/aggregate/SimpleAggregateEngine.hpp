#pragma once

#include <optional>
#include <algorithm>

#include "ppa/core/engines.hpp"

namespace ppa {

class SimpleAggregateEngine final : public AggregateEngine {
public:
    AggregateResult combine(const PreflightReport& preflight,
                            const std::optional<SemanticResult>& semantic) override {
        const auto technical_base = (preflight.technical_scores.technical_excellence * 0.4) +
                                    (preflight.technical_scores.color_balance * 0.2) +
                                    (preflight.technical_scores.lighting * 0.2) +
                                    (preflight.technical_scores.composition * 0.2);

        auto semantic_adjustment = 0.0;
        auto confidence = 0.15;
        if (semantic.has_value() && !semantic->votes.empty()) {
            const auto& vote = semantic->votes.front();
            const auto vote_direction = vote.vote == "C" ? 1.0 : -1.0;
            semantic_adjustment = vote_direction * (12.0 * std::clamp(vote.confidence, 0.0, 1.0));
            confidence = 0.35 + (0.45 * std::clamp(vote.confidence, 0.0, 1.0));
        }

        const auto merit_score = std::clamp(technical_base + semantic_adjustment, 0.0, 100.0);

        AggregateResult result;
        result.classification = preflight.status == "pass" ? "C" : "D";
        result.merit_score = merit_score;
        result.merit_probability = merit_score / 100.0;
        result.confidence = std::clamp(confidence, 0.0, 1.0);
        result.summary = semantic.has_value() ? semantic->summary : "stub critique response";
        return result;
    }
};

}  // namespace ppa
