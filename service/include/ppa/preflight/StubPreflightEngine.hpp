#pragma once

#include <string>
#include <algorithm>

#include "ppa/core/engines.hpp"

namespace ppa {

class StubPreflightEngine final : public PreflightEngine {
public:
    PreflightReport run(const CritiqueRequest& request) override {
        const auto has_dimensions = request.metadata.width > 0 && request.metadata.height > 0;
        const auto total_pixels = static_cast<long long>(request.metadata.width) * static_cast<long long>(request.metadata.height);
        const auto has_profile = !request.metadata.icc_profile.empty() && request.metadata.icc_profile != "unknown";
        const auto rating_bonus = std::clamp(request.metadata.rating, 0, 5) * 2.0;

        auto technical = 40.0;
        if (total_pixels >= 7000000) {
            technical = 82.0;
        } else if (total_pixels >= 3000000) {
            technical = 74.0;
        } else if (total_pixels > 0) {
            technical = 62.0;
        }
        if (has_profile) {
            technical += 4.0;
        }
        technical = std::min(technical + rating_bonus, 95.0);

        auto color_balance = has_profile ? 76.0 : 58.0;
        auto lighting = has_dimensions ? 72.0 : 46.0;
        auto composition = 58.0 + rating_bonus;
        if (!request.metadata.keywords.empty()) {
            composition += 4.0;
        }
        composition = std::min(composition, 90.0);

        return PreflightReport{
            .status = has_dimensions ? "pass" : "warn",
            .checks = {PreflightCheck{.id = "dimensions",
                                      .result = has_dimensions ? "pass" : "warn",
                                      .message = has_dimensions ? "stub dimensions check completed"
                                                                : "dimensions missing from request"}},
            .technical_scores =
                TechnicalScores{
                    .technical_excellence = technical,
                    .color_balance = color_balance,
                    .lighting = lighting,
                    .composition = composition,
                },
        };
    }
};

}  // namespace ppa
