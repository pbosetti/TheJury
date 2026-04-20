#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ppa/api/ApiError.hpp"
#include "ppa/api/Handlers.hpp"
#include "ppa/config/ServiceConfig.hpp"
#include "ppa/runtime/RuntimeManager.hpp"
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

class DelayedOllamaTransport final : public ppa::OllamaTransport {
public:
    explicit DelayedOllamaTransport(std::chrono::milliseconds delay, ppa::OllamaHttpResponse response)
        : _delay(delay),
          _response(std::move(response)) {}

    [[nodiscard]] ppa::OllamaHttpResponse get(const std::string&) const override {
        return ppa::OllamaHttpResponse{
            .status = 200,
            .body = json{{"models", json::array({json{{"name", "delayed-model"}}})}}.dump(),
            .error = "",
        };
    }

    [[nodiscard]] ppa::OllamaHttpResponse post(const std::string&, const std::string&) const override {
        std::this_thread::sleep_for(_delay);
        return _response;
    }

private:
    std::chrono::milliseconds _delay;
    ppa::OllamaHttpResponse _response;
};

class TestServer {
public:
    explicit TestServer(ppa::CritiqueService& service, std::filesystem::path config_path = std::filesystem::temp_directory_path() / "ppa_service.toml")
        : _config_path(std::move(config_path)),
          _runtime_manager(ppa::RuntimeManager::Options{
              .default_lease_ttl_seconds = 1,
              .shutdown_grace_seconds = 1,
              .monitor_interval_ms = 50,
          }) {
        ppa::api::register_routes(_server, service, _runtime_manager, _config_path);
        _port = _server.bind_to_any_port("127.0.0.1");
        if (_port <= 0) {
            throw std::runtime_error("failed to bind test server");
        }
        _worker = std::thread([this] { _server.listen_after_bind(); });
        _server.wait_until_ready();
        _runtime_manager.set_stop_callback([this] { _server.stop(); });
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
    ppa::RuntimeManager _runtime_manager;
};

std::string make_chat_response(const json& semantic_json) {
    return json{
        {"model", "qwen2.5vl:7b"},
        {"message", {{"role", "assistant"}, {"content", semantic_json.dump()}}},
        {"done", true},
    }
        .dump();
}

std::vector<ppa::JurorDefinition> make_jurors(std::initializer_list<const char*> ids) {
    auto jurors = std::vector<ppa::JurorDefinition>{};
    for (const auto* id : ids) {
        jurors.push_back(ppa::JurorDefinition{
            .judge_id = id,
            .personality = std::string("Personality for ") + id,
            .weight = 1.0,
        });
    }
    return jurors;
}

json make_panel_votes(std::initializer_list<std::tuple<const char*, const char*, double, const char*>> votes) {
    auto result = json::array();
    for (const auto& [judge_id, vote, confidence, rationale] : votes) {
        result.push_back(json{
            {"judge_id", judge_id},
            {"vote", vote},
            {"confidence", confidence},
            {"rationale", rationale},
        });
    }
    return result;
}

bool wait_for_jobs_in_flight(httplib::Client& client, int expected_jobs, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto response = client.Get("/v1/runtime/status");
        if (response && response->status == 200) {
            const auto body = json::parse(response->body);
            if (body.at("jobs_in_flight") == expected_jobs) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
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
            "semantic_provider": "disabled",
            "selected_jurors": [1, 3, 2]
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
    CHECK(request.options.selected_jurors == std::vector<int>{1, 3, 2});
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
    CHECK(loaded.config.jurors.size() == 5);
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
    output << "[[jurors]]\n";
    output << "judge_id = \"JX\"\n";
    output << "personality = \"Custom juror\"\n";
    output << "weight = 2.5\n";
    output.close();

    const auto loaded = ppa::load_service_config(temp_dir.path());
    CHECK(loaded.from_file);
    CHECK(loaded.config.ollama.base_url == "http://127.0.0.1:22434");
    CHECK(loaded.config.ollama.model == "model-a");
    CHECK(loaded.config.ollama.fallback_model == "model-b");
    CHECK(loaded.config.ollama.timeout_ms == 30000);
    CHECK(loaded.config.semantic.default_provider == "disabled");
    REQUIRE(loaded.config.jurors.size() == 1);
    CHECK(loaded.config.jurors[0].judge_id == "JX");
    CHECK(loaded.config.jurors[0].weight == 2.5);
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
    config.jurors = make_jurors({"JA", "JB"});
    config.jurors[0].weight = 1.5;

    ppa::write_service_config(config_path, config);

    const auto loaded = ppa::load_service_config(temp_dir.path());
    CHECK(loaded.from_file);
    CHECK(loaded.config.ollama.base_url == "http://127.0.0.1:22434");
    CHECK(loaded.config.ollama.model == "model-a");
    CHECK(loaded.config.ollama.fallback_model == "model-b");
    CHECK(loaded.config.ollama.timeout_ms == 30000);
    CHECK(loaded.config.semantic.default_provider == "disabled");
    REQUIRE(loaded.config.jurors.size() == 2);
    CHECK(loaded.config.jurors[0].judge_id == "JA");
    CHECK(loaded.config.jurors[0].weight == 1.5);
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
                                    {"votes", make_panel_votes({
                                        {"J1", "C", 0.7, "Strong composition."},
                                        {"J2", "D", 0.4, "Weak impact."},
                                    })},
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
    CHECK(output.result.votes.size() == 2);

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
                                    {"votes", make_panel_votes({
                                        {"J1", "C", 0.7, "Strong composition."},
                                        {"J2", "C", 0.8, "Good impact."},
                                        {"J3", "D", 0.5, "Needs refinement."},
                                    })},
                                    {"strengths", json::array({"Impact"})},
                                    {"improvements", json::array({"Refine crop"})},
                                }),
                                .error = ""});

    auto config = ppa::ServiceConfig{};
    config.ollama.model = "primary-model";
    config.ollama.fallback_model = "secondary-model";
    config.jurors = make_jurors({"J1", "J2", "J3"});

    auto request = semantic_request();
    request.image.path = image_path.string();

    auto service = ppa::CritiqueService(config, ppa::OllamaClient(config.ollama, transport));
    const auto response = ppa::api::critique_payload(service, request);

    REQUIRE(response.semantic.has_value());
    CHECK(response.runtime.semantic_provider == "ollama");
    CHECK(response.runtime.model == "primary-model");
    CHECK(response.semantic->votes.size() == 3);
    CHECK(response.aggregate.classification == "C");
    CHECK(response.aggregate.merit_score > 0.0);
    CHECK(response.aggregate.summary == "semantic summary");
}

TEST_CASE("semantic critique honors selected juror subset and order") {
    const auto temp_dir = TemporaryDirectory{};
    const auto image_path = temp_dir.path() / "photo.jpg";
    auto image = std::ofstream(image_path, std::ios::binary);
    image << "jpeg-bytes";
    image.close();

    auto transport = std::make_shared<MockOllamaTransport>();
    transport->post_responses.push_back(
        ppa::OllamaHttpResponse{.status = 200,
                                .body = make_chat_response(json{
                                    {"summary", "subset semantic summary"},
                                    {"votes", make_panel_votes({
                                        {"J1", "C", 0.7, "Juror one rationale."},
                                        {"J3", "D", 0.8, "Juror three rationale."},
                                        {"J2", "C", 0.6, "Juror two rationale."},
                                    })},
                                    {"strengths", json::array({"Impact"})},
                                    {"improvements", json::array({"Refine crop"})},
                                }),
                                .error = ""});

    auto config = ppa::ServiceConfig{};
    config.ollama.model = "primary-model";
    config.ollama.fallback_model = "secondary-model";
    config.jurors = make_jurors({"J1", "J2", "J3"});

    auto request = semantic_request();
    request.image.path = image_path.string();
    request.options.selected_jurors = {1, 3, 2};

    auto service = ppa::CritiqueService(config, ppa::OllamaClient(config.ollama, transport));
    const auto response = ppa::api::critique_payload(service, request);

    REQUIRE(response.semantic.has_value());
    REQUIRE(response.semantic->votes.size() == 3);
    CHECK(response.semantic->votes[0].judge_id == "J1");
    CHECK(response.semantic->votes[1].judge_id == "J3");
    CHECK(response.semantic->votes[2].judge_id == "J2");
    CHECK(response.aggregate.summary == "subset semantic summary");
}

TEST_CASE("semantic critique rejects out-of-range selected jurors") {
    auto config = ppa::ServiceConfig{};
    config.jurors = make_jurors({"J1", "J2", "J3"});

    auto request = semantic_request();
    request.options.selected_jurors = {4};

    auto service = ppa::CritiqueService(config);

    try {
        (void)ppa::api::critique_payload(service, request);
        FAIL("expected invalid_request ApiError");
    } catch (const ppa::api::ApiError& error) {
        CHECK(error.status() == 400);
        CHECK(error.code() == "invalid_request");
        CHECK(std::string(error.what()).find("selected_jurors") != std::string::npos);
    }
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

TEST_CASE("runtime status endpoint returns live runtime state") {
    auto service = ppa::CritiqueService{};
    auto server = TestServer(service);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto response = client.Get("/v1/runtime/status");

    REQUIRE(response);
    REQUIRE(response->status == 200);

    const auto body = json::parse(response->body);
    CHECK(body.at("state") == "running");
    CHECK(body.at("reachable") == true);
    CHECK(body.at("service") == "ppa-companion");
    CHECK(body.at("version") == "0.1.0");
    CHECK(body.at("active_lease_count") == 0);
    CHECK(body.at("jobs_in_flight") == 0);
    CHECK(body.at("provider") == "ollama");
}

TEST_CASE("runtime lease endpoint renews and releases leases") {
    auto service = ppa::CritiqueService{};
    auto server = TestServer(service);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto lease_response =
        client.Put("/v1/runtime/lease",
                   json{{"client", "com.pbosetti.thejury"}, {"instance_id", "instance-1"}, {"ttl_seconds", 2}}.dump(),
                   "application/json");

    REQUIRE(lease_response);
    REQUIRE(lease_response->status == 200);
    const auto lease_body = json::parse(lease_response->body);
    CHECK(lease_body.at("state") == "running");
    CHECK(lease_body.at("expires_in_seconds") == 2);
    CHECK(lease_body.at("active_lease_count") == 1);

    const auto status_response = client.Get("/v1/runtime/status");
    REQUIRE(status_response);
    REQUIRE(status_response->status == 200);
    CHECK(json::parse(status_response->body).at("active_lease_count") == 1);

    const auto release_response = client.Delete("/v1/runtime/lease/instance-1");
    REQUIRE(release_response);
    REQUIRE(release_response->status == 200);
    const auto release_body = json::parse(release_response->body);
    CHECK(release_body.at("state") == "running");
    CHECK(release_body.at("active_lease_count") == 0);
}

TEST_CASE("runtime lease expiry stops the server after the grace period") {
    auto service = ppa::CritiqueService{};
    auto server = TestServer(service);
    auto client = httplib::Client("127.0.0.1", server.port());

    const auto lease_response =
        client.Put("/v1/runtime/lease",
                   json{{"client", "com.pbosetti.thejury"}, {"instance_id", "instance-2"}, {"ttl_seconds", 1}}.dump(),
                   "application/json");
    REQUIRE(lease_response);
    REQUIRE(lease_response->status == 200);

    std::this_thread::sleep_for(std::chrono::milliseconds(2400));
    CHECK_FALSE(client.Get("/health"));
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
    config.jurors = make_jurors({"JA", "JB"});

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
    REQUIRE(body.at("jurors").size() == 2);
    CHECK(body.at("jurors").at(0).at("judge_id") == "JA");
    CHECK(body.at("available_models").size() == 2);
    CHECK(body.at("path") == config_path.string());
}

TEST_CASE("config endpoint updates live service config and persists TOML") {
    const auto temp_dir = TemporaryDirectory{};
    const auto config_path = temp_dir.path() / "ppa_service.toml";

    auto transport = std::make_shared<MockOllamaTransport>();
    auto initialConfig = ppa::ServiceConfig{};
    initialConfig.jurors = make_jurors({"JX"});
    auto service = ppa::CritiqueService(initialConfig, ppa::OllamaClient(ppa::OllamaSettings{}, transport));
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
    REQUIRE(loaded.config.jurors.size() == 1);
    CHECK(loaded.config.jurors[0].judge_id == "JX");
}

TEST_CASE("critique endpoint returns error payload when semantic backend is unavailable") {
    const auto temp_dir = TemporaryDirectory{};
    const auto image_path = temp_dir.path() / "photo.jpg";
    auto image = std::ofstream(image_path, std::ios::binary);
    image << "jpeg-bytes";
    image.close();

    auto transport = std::make_shared<MockOllamaTransport>();
    transport->post_responses.push_back(
        ppa::OllamaHttpResponse{.status = 0, .body = "", .error = "connection refused"});

    auto service =
        ppa::CritiqueService(ppa::ServiceConfig{}, ppa::OllamaClient(ppa::OllamaSettings{}, transport));
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

TEST_CASE("runtime manager rejects new jobs once shutdown starts") {
    auto service = ppa::CritiqueService{};
    auto runtime_manager = ppa::RuntimeManager(ppa::RuntimeManager::Options{
        .default_lease_ttl_seconds = 1,
        .shutdown_grace_seconds = 1,
        .monitor_interval_ms = 20,
    });

    auto job = runtime_manager.try_begin_job();
    REQUIRE(job);

    const auto shutdown_status = runtime_manager.request_shutdown(service);
    CHECK(shutdown_status.state == "draining");
    CHECK(shutdown_status.jobs_in_flight == 1);

    auto rejected_job = runtime_manager.try_begin_job();
    CHECK_FALSE(rejected_job);
}

TEST_CASE("runtime manager delays stop until active jobs finish") {
    auto service = ppa::CritiqueService{};
    auto runtime_manager = ppa::RuntimeManager(ppa::RuntimeManager::Options{
        .default_lease_ttl_seconds = 1,
        .shutdown_grace_seconds = 1,
        .monitor_interval_ms = 20,
    });

    auto stop_requested = false;
    runtime_manager.set_stop_callback([&stop_requested] { stop_requested = true; });

    auto job = runtime_manager.try_begin_job();
    REQUIRE(job);

    const auto shutdown_status = runtime_manager.request_shutdown(service);
    CHECK(shutdown_status.state == "draining");
    CHECK(shutdown_status.jobs_in_flight == 1);
    CHECK_FALSE(stop_requested);

    job = ppa::RuntimeManager::JobGuard{};
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    CHECK(stop_requested);
}
