#pragma once

#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ppa/api/ApiError.hpp"
#include "ppa/config/ServiceConfig.hpp"
#include "ppa/core/engines.hpp"
#include "ppa/semantic/OllamaClient.hpp"

namespace ppa {

class OllamaProvider final : public SemanticProvider {
public:
    explicit OllamaProvider(OllamaClient client = OllamaClient{}, std::vector<JurorDefinition> jurors = {})
        : _client(std::move(client)), _jurors(std::move(jurors)) {}

    SemanticOutput evaluate(const CritiqueRequest& request, const PreflightReport& preflight) override {
        std::ostringstream prompt;
        prompt << "You are coordinating a five-juror PPA image critique panel. "
               << "Review the supplied image and respond with valid JSON only. "
               << "Return an object with keys: summary, votes, strengths, improvements. "
               << "votes must contain exactly " << _jurors.size()
               << " objects with keys judge_id, vote, confidence, rationale. "
               << "vote must be either C or D. "
               << "Use concise professional language. "
               << "Each juror must reflect the configured personality in the rationale.\n"
               << "Photo file: " << request.photo.file_name << "\n"
               << "Category: " << request.category << "\n"
               << "Mode: " << request.mode << "\n"
               << "Dimensions: " << request.metadata.width << "x" << request.metadata.height << "\n"
               << "ICC profile: " << request.metadata.icc_profile << "\n"
               << "Keywords: ";

        if (request.metadata.keywords.empty()) {
            prompt << "none";
        } else {
            for (std::size_t index = 0; index < request.metadata.keywords.size(); ++index) {
                if (index != 0) {
                    prompt << ", ";
                }
                prompt << request.metadata.keywords[index];
            }
        }

        prompt << "\nPreflight status: " << preflight.status << "\nChecks:\n";
        for (const auto& check : preflight.checks) {
            prompt << "- " << check.id << ": " << check.result << " - " << check.message << '\n';
        }

        prompt << "Jurors:\n";
        for (const auto& juror : _jurors) {
            prompt << "- " << juror.judge_id << " (weight " << juror.weight << "): " << juror.personality << '\n';
        }

        auto output = _client.evaluate(prompt.str(), request.image.path);
        validate_votes(output.result);
        return output;
    }

private:
    void validate_votes(SemanticResult& result) const {
        if (result.votes.size() != _jurors.size()) {
            throw api::ApiError(502,
                                "semantic_invalid_response",
                                "Ollama response must contain exactly " + std::to_string(_jurors.size()) + " judge votes");
        }

        auto seen = std::unordered_set<std::string>{};
        auto normalized_votes = std::vector<JudgeVote>{};
        normalized_votes.reserve(_jurors.size());

        for (const auto& juror : _jurors) {
            auto found = std::find_if(result.votes.begin(), result.votes.end(), [&](const JudgeVote& vote) {
                return vote.judge_id == juror.judge_id;
            });
            if (found == result.votes.end()) {
                throw api::ApiError(502,
                                    "semantic_invalid_response",
                                    "Ollama response is missing juror vote for " + juror.judge_id);
            }
            if (!seen.insert(found->judge_id).second) {
                throw api::ApiError(502, "semantic_invalid_response", "Ollama response contains duplicate juror ids");
            }
            normalized_votes.push_back(*found);
        }

        result.votes = std::move(normalized_votes);
    }

    OllamaClient _client;
    std::vector<JurorDefinition> _jurors;
};

}  // namespace ppa
