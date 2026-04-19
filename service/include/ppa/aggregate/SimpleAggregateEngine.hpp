#pragma once

#include <optional>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ppa/core/engines.hpp"
#include "ppa/config/ServiceConfig.hpp"

namespace ppa {

class SimpleAggregateEngine final : public AggregateEngine {
public:
    explicit SimpleAggregateEngine(std::vector<JurorDefinition> jurors = {})
        : _jurors(std::move(jurors)) {}

    AggregateResult combine(const PreflightReport& preflight,
                            const std::optional<SemanticResult>& semantic) override {
        const auto technical_base = (preflight.technical_scores.technical_excellence * 0.4) +
                                    (preflight.technical_scores.color_balance * 0.2) +
                                    (preflight.technical_scores.lighting * 0.2) +
                                    (preflight.technical_scores.composition * 0.2);

        auto semantic_adjustment = 0.0;
        auto confidence = 0.15;
        auto jury_classification = preflight.status == "pass" ? std::string{"C"} : std::string{"D"};
        if (semantic.has_value() && !semantic->votes.empty()) {
            auto weight_by_juror = std::unordered_map<std::string, double>{};
            auto total_weight = 0.0;
            auto weighted_vote = 0.0;
            auto weighted_confidence = 0.0;

            for (const auto& juror : _jurors) {
                weight_by_juror[juror.judge_id] = juror.weight;
            }

            for (const auto& vote : semantic->votes) {
                const auto weight = weight_by_juror.contains(vote.judge_id) ? weight_by_juror.at(vote.judge_id) : 1.0;
                const auto normalized_confidence = std::clamp(vote.confidence, 0.0, 1.0);
                total_weight += weight;
                weighted_confidence += weight * normalized_confidence;
                weighted_vote += weight * (vote.vote == "C" ? 1.0 : -1.0) * normalized_confidence;
            }

            if (total_weight > 0.0) {
                const auto mean_confidence = weighted_confidence / total_weight;
                semantic_adjustment = 18.0 * (weighted_vote / total_weight);
                confidence = 0.35 + (0.45 * mean_confidence);
                jury_classification = weighted_vote >= 0.0 ? "C" : "D";
            }
        }

        const auto merit_score = std::clamp(technical_base + semantic_adjustment, 0.0, 100.0);

        AggregateResult result;
        result.classification = preflight.status == "pass" ? jury_classification : "D";
        result.merit_score = merit_score;
        result.merit_probability = merit_score / 100.0;
        result.confidence = std::clamp(confidence, 0.0, 1.0);
        result.summary = semantic.has_value() ? semantic->summary : "stub critique response";
        return result;
    }

private:
    std::vector<JurorDefinition> _jurors;
};

}  // namespace ppa
