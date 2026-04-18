#pragma once

#include <utility>

#include "ppa/core/engines.hpp"
#include "ppa/semantic/OllamaClient.hpp"

namespace ppa {

class OllamaProvider final : public SemanticProvider {
public:
    explicit OllamaProvider(OllamaClient client = OllamaClient{}, std::string model = "qwen2.5vl:7b")
        : client_(std::move(client)), model_(std::move(model)) {}

    SemanticResult evaluate(const CritiqueRequest& request, const PreflightReport&) override {
        const auto availability = client_.is_available() ? "available" : "unavailable";
        return SemanticResult{
            .summary = "stub Ollama critique for " + request.photo.file_name + " (runtime " + availability + ")",
            .votes = {JudgeVote{.judge_id = "J1",
                                .vote = "C",
                                .confidence = 0.35,
                                .rationale = "Stub semantic provider; semantic scoring is not implemented yet."}},
            .strengths = {"Provider abstraction compiles and is wired into the critique flow."},
            .improvements = {"Install and configure Ollama to replace this stub response."},
        };
    }

    [[nodiscard]] const std::string& model() const { return model_; }

private:
    OllamaClient client_;
    std::string model_;
};

}  // namespace ppa
