#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>
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
        auto combined = SemanticResult{};
        auto model = std::string{};

        for (const auto& juror : _jurors) {
            const auto output = _client.evaluate(build_prompt_for_juror(request, preflight, juror), request.image.path);
            if (model.empty()) {
                model = output.model;
            }

            auto normalized_vote = normalize_single_juror_vote(output.result, juror);
            combined.votes.push_back(std::move(normalized_vote));
            append_unique_text(combined.summary, output.result.summary);
            append_unique_items(combined.strengths, output.result.strengths, 5);
            append_unique_items(combined.improvements, output.result.improvements, 5);
        }

        if (combined.summary.empty()) {
            combined.summary = "Individual juror critiques completed.";
        }

        return SemanticOutput{
            .result = std::move(combined),
            .model = model,
        };
    }

private:
    struct MeritElementDefinition {
        const char* name;
        const char* guidance;
    };

    static constexpr std::array<MeritElementDefinition, 12> kMeritElements{{
        {"Impact", "Judge the immediate emotional response and memorability."},
        {"Technical Excellence", "Judge sharpness, exposure, retouching, print quality, and overall technical polish."},
        {"Creativity", "Judge originality, freshness, and imaginative expression."},
        {"Style", "Judge whether stylistic choices reinforce the intended effect and subject."},
        {"Composition", "Judge arrangement, balance, and control of visual flow."},
        {"Presentation", "Judge whether finishing and display choices strengthen the image rather than distract."},
        {"Color Balance", "Judge whether color relationships support mood, harmony, or intentional tension."},
        {"Center of Interest", "Judge whether viewer attention is directed clearly to the intended focal point."},
        {"Lighting", "Judge quality, control, shape, mood, and dimensionality of light."},
        {"Subject Matter", "Judge whether the chosen subject supports the intended idea or story."},
        {"Technique", "Judge whether the maker's methods and execution effectively support the image."},
        {"Story Telling", "Judge what narrative, idea, or emotional story the image communicates."},
    }};

    static constexpr const std::array<MeritElementDefinition, 12>& merit_elements() {
        return kMeritElements;
    }

    static std::string join_keywords(const std::vector<std::string>& keywords) {
        if (keywords.empty()) {
            return "none";
        }

        auto joined = std::string{};
        for (std::size_t index = 0; index < keywords.size(); ++index) {
            if (index != 0) {
                joined += ", ";
            }
            joined += keywords[index];
        }
        return joined;
    }

    static void append_unique_text(std::string& target, const std::string& addition) {
        if (addition.empty()) {
            return;
        }
        if (target.find(addition) != std::string::npos) {
            return;
        }
        if (!target.empty()) {
            target += " ";
        }
        target += addition;
    }

    static void append_unique_items(std::vector<std::string>& target,
                                    const std::vector<std::string>& source,
                                    const std::size_t max_items) {
        for (const auto& item : source) {
            if (item.empty()) {
                continue;
            }
            if (std::find(target.begin(), target.end(), item) != target.end()) {
                continue;
            }
            target.push_back(item);
            if (target.size() >= max_items) {
                return;
            }
        }
    }

    static std::string build_prompt_for_juror(const CritiqueRequest& request,
                                              const PreflightReport& preflight,
                                              const JurorDefinition& juror) {
        auto prompt = std::ostringstream{};
        prompt << "You are juror " << juror.judge_id << " in a PPA Merit Image Review critique. "
               << "Your personality is: " << juror.personality << " "
               << "Review the supplied image against the 12 Elements of a Merit Image and respond with valid JSON only. "
               << "Return an object with keys: summary, votes, strengths, improvements. "
               << "votes must contain exactly 1 object with keys judge_id, vote, confidence, rationale, element_reviews. "
               << "vote must be either C or D. "
               << "The single vote.judge_id must be exactly " << juror.judge_id << ". "
               << "summary must be a concise overall juror synthesis of at most two sentences. "
               << "strengths must be 3 to 5 short distinct phrases, not sentences. "
               << "improvements must be 3 to 5 short distinct phrases, not sentences. "
               << "rationale must be an overall juror comment of at most two sentences. "
               << "element_reviews must contain exactly 12 objects with keys element and comment, using the exact PPA element names.\n"
               << "Photo file: " << request.photo.file_name << "\n"
               << "Category: " << request.category << "\n"
               << "Mode: " << request.mode << "\n"
               << "Dimensions: " << request.metadata.width << "x" << request.metadata.height << "\n"
               << "ICC profile: " << request.metadata.icc_profile << "\n"
               << "Keywords: " << join_keywords(request.metadata.keywords) << "\n"
               << "Preflight status: " << preflight.status << "\nChecks:\n";

        for (const auto& check : preflight.checks) {
            prompt << "- " << check.id << ": " << check.result << " - " << check.message << '\n';
        }

        prompt << "PPA 12 elements to address, in this exact order:\n";
        for (const auto& element : merit_elements()) {
            prompt << "- " << element.name << ": " << element.guidance << '\n';
        }

        return prompt.str();
    }

    static std::string canonicalize_element_name(std::string value) {
        auto canonical = std::string{};
        canonical.reserve(value.size());
        for (const auto character : value) {
            if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
                canonical.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
        }
        return canonical;
    }

    static bool matches_element_name(const std::string& candidate, const char* expected) {
        const auto normalized_candidate = canonicalize_element_name(candidate);
        const auto normalized_expected = canonicalize_element_name(expected);
        if (normalized_candidate == normalized_expected) {
            return true;
        }

        if (normalized_expected == "storytelling") {
            return normalized_candidate == "storytelling" || normalized_candidate == "storytellingelement";
        }
        if (normalized_expected == "centerofinterest") {
            return normalized_candidate == "centerofinterest" || normalized_candidate == "centreofinterest";
        }
        if (normalized_expected == "technicalexcellence") {
            return normalized_candidate == "technicalexcellence" || normalized_candidate == "technicalquality";
        }
        if (normalized_expected == "technique") {
            return normalized_candidate == "technique" || normalized_candidate == "techniques";
        }

        return false;
    }

    static std::vector<MeritElementReview> normalize_element_reviews(const JudgeVote& vote) {
        auto normalized = std::vector<MeritElementReview>{};
        normalized.reserve(merit_elements().size());
        auto seen = std::unordered_set<std::string>{};
        auto missing_elements = std::vector<std::string>{};

        for (const auto& element : merit_elements()) {
            const auto found = std::find_if(vote.element_reviews.begin(), vote.element_reviews.end(), [&](const MeritElementReview& review) {
                return matches_element_name(review.element, element.name);
            });
            if (found == vote.element_reviews.end()) {
                missing_elements.push_back(element.name);
                continue;
            }
            if (!seen.insert(canonicalize_element_name(element.name)).second) {
                throw api::ApiError(502,
                                    "semantic_invalid_response",
                                    "Ollama response contains duplicate merit-element reviews for " + vote.judge_id);
            }
            if (found->comment.empty()) {
                throw api::ApiError(502,
                                    "semantic_invalid_response",
                                    "Ollama response contains an empty merit-element comment for " + vote.judge_id + ": " + element.name);
            }
            normalized.push_back(MeritElementReview{
                .element = element.name,
                .comment = found->comment,
            });
        }

        if (missing_elements.size() == 1) {
            normalized.push_back(MeritElementReview{
                .element = missing_elements.front(),
                .comment = "Not explicitly covered by the model response; inferred follow-up needed for this element.",
            });
        } else if (!missing_elements.empty()) {
            throw api::ApiError(502,
                                "semantic_invalid_response",
                                "Ollama response is missing merit-element review for " + vote.judge_id + ": " + missing_elements.front());
        }

        if (normalized.size() != merit_elements().size()) {
            throw api::ApiError(502,
                                "semantic_invalid_response",
                                "Ollama response must contain exactly " + std::to_string(merit_elements().size()) +
                                    " merit-element reviews for " + vote.judge_id);
        }

        return normalized;
    }

    static JudgeVote normalize_vote_for_juror(const JudgeVote& source,
                                              const JurorDefinition& juror) {
        auto normalized = source;
        if (normalized.rationale.empty()) {
            throw api::ApiError(502,
                                "semantic_invalid_response",
                                "Ollama response contains an empty rationale for " + juror.judge_id);
        }
        normalized.judge_id = juror.judge_id;
        normalized.element_reviews = normalize_element_reviews(normalized);
        return normalized;
    }

    static JudgeVote normalize_single_juror_vote(const SemanticResult& result,
                                                 const JurorDefinition& juror) {
        if (result.votes.empty()) {
            throw api::ApiError(502,
                                "semantic_invalid_response",
                                "Ollama response must contain at least one judge vote");
        }

        const auto exact_match = std::find_if(result.votes.begin(), result.votes.end(), [&](const JudgeVote& vote) {
            return vote.judge_id == juror.judge_id;
        });
        if (exact_match != result.votes.end()) {
            return normalize_vote_for_juror(*exact_match, juror);
        }

        return normalize_vote_for_juror(result.votes.front(), juror);
    }

    OllamaClient _client;
    std::vector<JurorDefinition> _jurors;
};

}  // namespace ppa
