#pragma once

#include <optional>

#include "ppa/model.hpp"

namespace ppa {

class PreflightEngine {
public:
    virtual ~PreflightEngine() = default;
    virtual PreflightReport run(const CritiqueRequest& request) = 0;
};

class SemanticProvider {
public:
    virtual ~SemanticProvider() = default;
    virtual SemanticResult evaluate(const CritiqueRequest& request,
                                    const PreflightReport& preflight) = 0;
};

class AggregateEngine {
public:
    virtual ~AggregateEngine() = default;
    virtual AggregateResult combine(const PreflightReport& preflight,
                                    const std::optional<SemanticResult>& semantic) = 0;
};

}  // namespace ppa
