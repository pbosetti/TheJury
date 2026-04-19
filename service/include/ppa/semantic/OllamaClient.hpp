#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ppa/config/ServiceConfig.hpp"
#include "ppa/model.hpp"

namespace ppa {

struct OllamaHttpResponse {
    int status{0};
    std::string body;
    std::string error;
};

class OllamaTransport {
public:
    virtual ~OllamaTransport() = default;
    [[nodiscard]] virtual OllamaHttpResponse get(const std::string& path) const = 0;
    [[nodiscard]] virtual OllamaHttpResponse post(const std::string& path, const std::string& body) const = 0;
};

class OllamaClient {
public:
    explicit OllamaClient(OllamaSettings settings = OllamaSettings{});
    OllamaClient(OllamaSettings settings, std::shared_ptr<OllamaTransport> transport);

    [[nodiscard]] std::vector<std::string> configured_models() const;
    [[nodiscard]] std::vector<std::string> available_models() const;
    [[nodiscard]] std::string default_model() const;
    [[nodiscard]] bool is_available() const;
    [[nodiscard]] SemanticOutput evaluate(const std::string& prompt, const std::string& image_path) const;

private:
    OllamaSettings _settings;
    std::shared_ptr<OllamaTransport> _transport;
};

}  // namespace ppa
