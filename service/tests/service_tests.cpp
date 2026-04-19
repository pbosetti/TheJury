#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ppa/api/ApiError.hpp"
#include "ppa/api/Handlers.hpp"
#include "ppa/config/ServiceConfig.hpp"
#include "ppa/semantic/OllamaClient.hpp"

namespace {
using json = nlohmann::json;

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        _path = std::filesystem::temp_directory_path() /
                std::filesystem::path("thejury-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(_path);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return _path; }

private:
    std::filesystem::path _path;
};

class MockOllamaTransport final : public ppa::OllamaTransport {
public:
    mutable std::vector<ppa::OllamaHttpResponse> get_responses;
    mutable std::vector<ppa::OllamaHttpResponse> post_responses;
    mutable std::string last_get_path;
    mutable std::string last_post_path;
    mutable std::string last_post_body;

    [[nodiscard]] ppa::OllamaHttpResponse get(const std::string& path) const override {
        last_get_path = path;
        if (get_responses.empty()) {
            return ppa::OllamaHttpResponse{.status = 0, .body = "", .error = "no mocked GET response"};
        }
        const auto response = get_responses.front();
        get_responses.erase(get_responses.begin());
        return response;
    }

    [[nodiscard]] ppa::OllamaHttpResponse post(const std::string& path, const std::string& body) const override {
        last_post_path = path;
        last_post_body = body;
        if (post_responses.empty()) {
            return ppa::OllamaHttpResponse{.status = 0, .body = "", .error = "no mocked POST response"};
        }
        const auto response = post_responses.front();
        post_responses.erase(post_responses.begin());
        return response;
    }
};

class TestServer {
public:
    explicit TestServer(ppa::CritiqueService& service, std::filesystem::path config_path = std::filesystem::temp_directory_path() / "ppa_service.toml")
        : _config_path(std::move(config_path)) {
        ppa::api::register_routes(_server, service, _config_path);
        _port = _server.bind_to_any_port("127.0.0.1");
        if (_port <= 0) {
            throw std::runtime_error("failed to bind test server");
        }
        _worker = std::thread([this] { _server.listen_after_bind(); });
        _server.wait_until_ready();
    }

    ~TestServer() {
        _server.stop();
        if (_worker.joinable()) {
            _worker.join();
        }
    }

    [[nodiscard]] int port() const { return _port; }

private:
    httplib::Server _server;
    int _port{0};
    std::thread _worker;
    std::filesystem::path _config_path;
};

std::string make_chat_response(const json& semantic_json) {
    return json{
        {"model", "qwen2.5vl:7b"},
        {"message", {{"role", "assistant"}, {"content", semantic_json.dump()}}},
        {"done", true},
    }
        .dump();
}

ppa::CritiqueRequest semantic_request() {
    return ppa::CritiqueRequest{
        .image = ppa::ImageInput{.path = "/tmp/photo.jpg"},
        .photo = ppa::PhotoInfo{.id = "1", .file_name = "photo.jpg"},
        .category = "illustrative",
        .mode = "mir12",
        .options = ppa::CritiqueOptions{.run_preflight = true, .run_semantic = true, .semantic_provider = "ollama"},
        .metadata =
            ppa::RequestMetadata{
                .width = 3840,
                .height = 2160,
                .icc_profile = "sRGB",
                .keywords = {"portrait"},
            },
    };
}

}  // namespace

TEST_CASE("critique requests deserialize from json") {
    const auto payload = json::parse(R"({
        "image": {"path": "/tmp/ppa-critique/photo-001.jpg"},
        "photo": {"id": "lr-photo-001", "file_name": "photo-001.jpg"},
        "category": "illustrative",
        "mode": "mir12",
        "options": {
            "run_preflight": true,
            "run_semantic": false,
            "semantic_provider": "disabled"
        },
        "metadata": {
            "width": 3840,
            "height": 2160,
            "icc_profile": "sRGB",
            "original_path": "/photos/library/photo-001.cr3",
            "capture_time": "2026-04-19 10:30:00",
            "file_format": "RAW",
            "color_label": "Red",
            "rating": 4,
            "keywords": ["portrait"]
        }
    })");

    const auto request = payload.get<ppa::CritiqueRequest>();

    CHECK(request.image.path == "/tmp/ppa-critique/photo-001.jpg");
    CHECK(request.photo.id == "lr-photo-001");
    CHECK(request.options.semantic_provider == "disabled");
    CHECK(request.metadata.original_path == "/photos/library/photo-001.cr3");
    CHECK(request.metadata.rating == 4);
    CHECK(request.metadata.keywords == std::vector<std::string>{"portrait"});
}

TEST_CASE("service config defaults apply when config file is missing") {
    const auto temp_dir = TemporaryDirectory{};
    const auto loaded = ppa::load_service_config(temp_dir.path());

    CHECK_FALSE(loaded.from_file);
    CHECK(loaded.config.ollama.base_url == "http://127.0.0.1:11434");
    CHECK(loaded.config.ollama.model == "qwen2.5vl:7b");
    CHECK(loaded.config.ollama.fallback_model == "qwen2.5vl:3b");
    CHECK(loaded.config.ollama.timeout_ms == 120000);
    CHECK(loaded.config.semantic.default_provider == "ollama");
}

TEST_CASE("service config overrides defaults from TOML") {
    const auto temp_dir = TemporaryDirectory{};
    auto output = std::ofstream(temp_dir.path() / "ppa_service.toml");
    output << "[ollama]\n";
    output << "base_url = \"http://127.0.0.1:22434\"\n";
    output << "model = \"model-a\"\n";
    output << "fallback_model = \"model-b\"\n";
    output << "timeout_ms = 30000\n";
    output << "[semantic]\n";
    output << "default_provider = \"disabled\"\n";
    output.close();

    const auto loaded = ppa::load_service_config(temp_dir.path());
    CHECK(loaded.from_file);
    CHECK(loaded.config.ollama.base_url == "http://127.0.0.1:22434");
    CHECK(loaded.config.ollama.model == "model-a");
    CHECK(loaded.config.ollama.fallback_model == "model-b");
    CHECK(loaded.config.ollama.timeout_ms == 30000);
    CHECK(loaded.config.semantic.default_provider == "disabled");
}

TEST_CASE("invalid TOML config fails clearly") {
    const auto temp_dir = TemporaryDirectory{};
    auto output = std::ofstream(temp_dir.path() / "ppa_service.toml");
    output << "[ollama\n";
    output.close();

    CHECK_THROWS(ppa::load_service_config(temp_dir.path()));
}

TEST_CASE("service config writes TOML output") {
    const auto temp_dir = TemporaryDirectory{};
    const auto config_path = temp_dir.path() / "ppa_service.toml";

    auto config = ppa::ServiceConfig{};
    config.ollama.base_url = "http://127.0.0.1:22434";
    config.ollama.model = "model-a";
    config.ollama.fallback_model = "model-b";
    config.ollama.timeout_ms = 30000;
    config.semantic.default_provider = "disabled";

    ppa::write_service_config(config_path, config);

    const auto loaded = ppa::load_service_config(temp_dir.path());
    CHECK(loaded.from_file);
    CHECK(loaded.config.ollama.base_url == "http://127.0.0.1:22434");
    CHECK(loaded.config.ollama.model == "model-a");
    CHECK(loaded.config.ollama.fallback_model == "model-b");
    CHECK(loaded.config.ollama.timeout_ms == 30000);
    CHECK(loaded.config.semantic.default_provider == "disabled");
}

TEST_CASE("ollama availability probe succeeds against mocked transport") {
    auto transport = std::make_shared<MockOllamaTransport>();
    transport->get_responses.push_back(ppa::OllamaHttpResponse{.status = 200, .body = "{}", .error = ""});

    const auto client = ppa::OllamaClient(ppa::OllamaSettings{}, transport);
    CHECK(client.is_available());
    CHECK(transport->last_get_path == "/api/tags");
}

TEST_CASE("ollama client evaluates image request and falls back to secondary model") {
    const auto temp_dir = TemporaryDirectory{};
    const auto image_path = temp_dir.path() / "photo.jpg";
    auto image = std::ofstream(image_path, std::ios::binary);
    image << "jpeg-bytes";
    image.close();

    auto transport = std::make_shared<MockOllamaTransport>();
    transport->post_responses.push_back(ppa::OllamaHttpResponse{.status = 404, .body = "", .error = ""});
    transport->post_responses.push_back(
        ppa::OllamaHttpResponse{.status = 200,
                                .body = make_chat_response(json{
                                    {"summary", "semantic summary"},
                                    {"votes",
                                     json::array({json{{"judge_id", "J1"},
                                                       {"vote", "C"},
                                                       {"confidence", 0.7},
                                                       {"rationale", "Strong composition."}}})},
                                    {"strengths", json::array({"Impact"})},
                                    {"improvements", json::array({"Refine crop"})},
                                }),
                                .error = ""});

    auto settings = ppa::OllamaSettings{};
    settings.model = "primary-model";
    settings.fallback_model = "secondary-model";
    const auto client = ppa::OllamaClient(settings, transport);

    const auto output = client.evaluate("Prompt text", image_path.string());
    CHECK(output.model == "secondary-model");
    CHECK(output.result.summary == "semantic summary");
    CHECK(output.result.votes.size() == 1);

    const auto request_json = json::parse(transport->last_post_body);
    CHECK(transport->last_post_path == "/api/chat");
    CHECK(request_json.at("model") == "secondary-model");
    CHECK(request_json.at("messages").at(0).at("images").size() == 1);
}

TEST_CASE("invalid Ollama output is treated as an error") {
    const auto temp_dir = TemporaryDirectory{};
    const auto image_path = temp_dir.path() / "photo.jpg";
    auto image = std::ofstream(image_path, std::ios::binary);
    image << "jpeg-bytes";
    image.close();

    auto transport = std::make_shared<MockOllamaTransport>();
    transport->post_responses.push_back(
        ppa::OllamaHttpResponse{.status = 200,
                                .body = json{{"message", {{"content", "{not-json}"}}}}.dump(),
                                .error = ""});

    const auto client = ppa::OllamaClient(ppa::OllamaSettings{}, transport);
    CHECK_THROWS_AS(client.evaluate("Prompt text", image_path.string()), ppa::api::ApiError);
}

TEST_CASE("aggregate stub serializes to expected shape") {
    auto service = ppa::CritiqueService{};
    const auto response = ppa::api::critique_payload(service,
                                                     ppa::CritiqueRequest{
                                                         .image = ppa::ImageInput{.path = "/tmp/photo.jpg"},
                                                         .photo = ppa::PhotoInfo{.id = "1", .file_name = "photo.jpg"},
                                                         .category = "illustrative",
                                                         .mode = "mir12",
                                                         .options = ppa::CritiqueOptions{},
                                                         .metadata = ppa::RequestMetadata{.width = 3840,
                                                                                          .height = 2160,
                                                                                          .icc_profile = "sRGB",
                                                                                          .keywords = {}},
                                                     });

    const auto json_response = json(response);

    CHECK(json_response.at("aggregate").at("classification") == "C");
    CHECK(json_response.at("aggregate").at("merit_score").get<double>() > 0.0);
    CHECK(json_response.at("aggregate").at("summary") == "stub critique response");
    CHECK(json_response.at("semantic").is_null());
}

TEST_CASE("semantic critique returns populated response when Ollama succeeds") {
    const auto temp_dir = TemporaryDirectory{};
    const auto image_path = temp_dir.path() / "photo.jpg";
    auto image = std::ofstream(image_path, std::ios::binary);
    image << "jpeg-bytes";
    image.close();

    auto transport = std::make_shared<MockOllamaTransport>();
    transport->post_responses.push_back(
        ppa::OllamaHttpResponse{.status = 200,
                                .body = make_chat_response(json{
                                    {"summary", "semantic summary"},
                                    {"votes",
                                     json::array({json{{"judge_id", "J1"},
                                                       {"vote", "C"},
                                                       {"confidence", 0.7},
                                                       {"rationale", "Strong composition."}}})},
                                    {"strengths", json::array({"Impact"})},
                                    {"improvements", json::array({"Refine crop"})},
                                }),
                                .error = ""});

    auto config = ppa::ServiceConfig{};
    config.ollama.model = "primary-model";
    config.ollama.fallback_model = "secondary-model";

    auto request = semantic_request();
    request.image.path = image_path.string();

    auto service = ppa::CritiqueService(config, ppa::OllamaClient(config.ollama, transport));
    const auto response = ppa::api::critique_payload(service, request);

    REQUIRE(response.semantic.has_value());
    CHECK(response.runtime.semantic_provider == "ollama");
    CHECK(response.runtime.model == "primary-model");
    CHECK(response.semantic->votes.size() == 1);
    CHECK(response.aggregate.merit_score > 0.0);
    CHECK(response.aggregate.summary == "semantic summary");
}

TEST_CASE("semantic critique throws when Ollama is unavailable") {
    const auto temp_dir = TemporaryDirectory{};
    const auto image_path = temp_dir.path() / "photo.jpg";
    auto image = std::ofstream(image_path, std::ios::binary);
    image << "jpeg-bytes";
    image.close();

    auto transport = std::make_shared<MockOllamaTransport>();
    transport->post_responses.push_back(ppa::OllamaHttpResponse{.status = 0, .body = "", .error = "connection refused"});

    auto request = semantic_request();
    request.image.path = image_path.string();

    auto service = ppa::CritiqueService(ppa::ServiceConfig{}, ppa::OllamaClient(ppa::OllamaSettings{}, transport));
    CHECK_THROWS_AS(ppa::api::critique_payload(service, request), ppa::api::ApiError);
}

TEST_CASE("health endpoint returns ok payload") {
    auto service = ppa::CritiqueService{};
    auto server = TestServer(service);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto response = client.Get("/health");

    REQUIRE(response);
    REQUIRE(response->status == 200);
    CHECK(json::parse(response->body) == json{{"status", "ok"}});
}

TEST_CASE("capabilities endpoint returns provider list") {
    auto service = ppa::CritiqueService{};
    auto server = TestServer(service);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto response = client.Get("/v1/capabilities");

    REQUIRE(response);
    REQUIRE(response->status == 200);

    const auto body = json::parse(response->body);
    REQUIRE(body.at("semantic").at("providers").size() == 2);
    CHECK(body.at("semantic").at("default_provider") == "ollama");
    CHECK(body.at("semantic").at("providers").at(0).at("name") == "disabled");
    CHECK(body.at("semantic").at("providers").at(1).at("name") == "ollama");
}

TEST_CASE("config endpoint returns live service config") {
    const auto temp_dir = TemporaryDirectory{};
    const auto config_path = temp_dir.path() / "ppa_service.toml";

    auto config = ppa::ServiceConfig{};
    config.ollama.model = "primary-model";
    config.ollama.fallback_model = "fallback-model";

    auto transport = std::make_shared<MockOllamaTransport>();
    transport->get_responses.push_back(ppa::OllamaHttpResponse{
        .status = 200,
        .body = json{{"models", json::array({json{{"name", "installed-a"}}, json{{"name", "installed-b"}}})}}.dump(),
        .error = "",
    });

    auto service = ppa::CritiqueService(config, ppa::OllamaClient(config.ollama, transport));
    auto server = TestServer(service, config_path);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto response = client.Get("/v1/config");

    REQUIRE(response);
    REQUIRE(response->status == 200);

    const auto body = json::parse(response->body);
    CHECK(body.at("ollama").at("model") == "primary-model");
    CHECK(body.at("ollama").at("fallback_model") == "fallback-model");
    CHECK(body.at("available_models").size() == 2);
    CHECK(body.at("path") == config_path.string());
}

TEST_CASE("config endpoint updates live service config and persists TOML") {
    const auto temp_dir = TemporaryDirectory{};
    const auto config_path = temp_dir.path() / "ppa_service.toml";

    auto transport = std::make_shared<MockOllamaTransport>();
    auto service = ppa::CritiqueService(ppa::ServiceConfig{}, ppa::OllamaClient(ppa::OllamaSettings{}, transport));
    auto server = TestServer(service, config_path);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto payload = json{
        {"ollama",
         {{"base_url", "http://127.0.0.1:22434"},
          {"model", "model-a"},
          {"fallback_model", "model-b"},
          {"timeout_ms", 45000}}},
        {"semantic", {{"default_provider", "disabled"}}},
    };

    const auto response = client.Put("/v1/config", payload.dump(), "application/json");

    REQUIRE(response);
    REQUIRE(response->status == 200);

    const auto body = json::parse(response->body);
    CHECK(body.at("ollama").at("model") == "model-a");
    CHECK(body.at("semantic").at("default_provider") == "disabled");

    const auto loaded = ppa::load_service_config(temp_dir.path());
    CHECK(loaded.config.ollama.base_url == "http://127.0.0.1:22434");
    CHECK(loaded.config.ollama.model == "model-a");
    CHECK(loaded.config.ollama.fallback_model == "model-b");
    CHECK(loaded.config.ollama.timeout_ms == 45000);
    CHECK(loaded.config.semantic.default_provider == "disabled");
}

TEST_CASE("critique endpoint returns error payload when semantic backend is unavailable") {
    const auto temp_dir = TemporaryDirectory{};
    const auto image_path = temp_dir.path() / "photo.jpg";
    auto image = std::ofstream(image_path, std::ios::binary);
    image << "jpeg-bytes";
    image.close();

    auto service = ppa::CritiqueService{};
    auto server = TestServer(service);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto request = json{
        {"image", {{"path", image_path.string()}}},
        {"photo", {{"id", "lr-photo-001"}, {"file_name", "photo-001.jpg"}}},
        {"category", "illustrative"},
        {"mode", "mir12"},
        {"options", {{"run_preflight", true}, {"run_semantic", true}, {"semantic_provider", "ollama"}}},
        {"metadata", {{"width", 3840}, {"height", 2160}, {"icc_profile", "sRGB"}, {"keywords", json::array({"portrait"})}}},
    };

    const auto response = client.Post("/v1/critique", request.dump(), "application/json");

    REQUIRE(response);
    REQUIRE(response->status >= 400);

    const auto body = json::parse(response->body);
    CHECK(body.at("error") == "semantic_unavailable");
}
