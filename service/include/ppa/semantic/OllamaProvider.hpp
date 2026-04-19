#pragma once

#include <sstream>
#include <utility>

#include "ppa/core/engines.hpp"
#include "ppa/semantic/OllamaClient.hpp"

namespace ppa {

class OllamaProvider final : public SemanticProvider {
public:
    explicit OllamaProvider(OllamaClient client = OllamaClient{}) : _client(std::move(client)) {}

    SemanticOutput evaluate(const CritiqueRequest& request, const PreflightReport& preflight) override {
        std::ostringstream prompt;
        prompt << "You are a PPA image critique assistant. "
               << "Review the supplied image and respond with valid JSON only. "
               << "Return an object with keys: summary, votes, strengths, improvements. "
               << "votes must contain exactly one object with keys judge_id, vote, confidence, rationale. "
               << "vote must be either C or D. "
               << "Use concise professional language.\n"
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

        return _client.evaluate(prompt.str(), request.image.path);
    }

private:
    OllamaClient _client;
};

}  // namespace ppa
