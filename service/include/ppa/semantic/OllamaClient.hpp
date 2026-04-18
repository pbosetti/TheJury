#pragma once

#include <string>
#include <vector>

#include "ppa/model.hpp"

namespace ppa {

class OllamaClient {
public:
    [[nodiscard]] std::vector<std::string> configured_models() const {
        return {"qwen2.5vl:7b", "qwen2.5vl:3b"};
    }

    [[nodiscard]] std::string default_model() const {
        return configured_models().front();
    }

    [[nodiscard]] bool is_available() const {
        return false;
    }
};

}  // namespace ppa
