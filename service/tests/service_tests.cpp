#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ppa/api/Handlers.hpp"

namespace {
using json = nlohmann::json;

class TestServer {
public:
    TestServer() {
        ppa::api::register_routes(server_);
        port_ = server_.bind_to_port("127.0.0.1", 0);
        worker_ = std::thread([this] { server_.listen_after_bind(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~TestServer() {
        server_.stop();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    [[nodiscard]] int port() const { return port_; }

private:
    httplib::Server server_;
    int port_{0};
    std::thread worker_;
};
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
            "keywords": ["portrait"]
        }
    })");

    const auto request = payload.get<ppa::CritiqueRequest>();

    CHECK(request.image.path == "/tmp/ppa-critique/photo-001.jpg");
    CHECK(request.photo.id == "lr-photo-001");
    CHECK(request.options.semantic_provider == "disabled");
    CHECK(request.metadata.keywords == std::vector<std::string>{"portrait"});
}

TEST_CASE("aggregate stub serializes to expected shape") {
    const auto response = ppa::api::critique_payload(ppa::CritiqueRequest{
        .image = ppa::ImageInput{.path = "/tmp/photo.jpg"},
        .photo = ppa::PhotoInfo{.id = "1", .file_name = "photo.jpg"},
        .category = "illustrative",
        .mode = "mir12",
        .options = ppa::CritiqueOptions{},
        .metadata = ppa::RequestMetadata{},
    });

    const auto json_response = json(response);

    CHECK(json_response.at("aggregate").at("classification") == "C");
    CHECK(json_response.at("aggregate").at("summary") == "stub critique response");
    CHECK(json_response.at("semantic").is_null());
}

TEST_CASE("health endpoint returns ok payload") {
    TestServer server;
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Get("/health");

    REQUIRE(response);
    REQUIRE(response->status == 200);
    CHECK(json::parse(response->body) == json{{"status", "ok"}});
}

TEST_CASE("capabilities endpoint returns provider list") {
    TestServer server;
    httplib::Client client("127.0.0.1", server.port());

    const auto response = client.Get("/v1/capabilities");

    REQUIRE(response);
    REQUIRE(response->status == 200);

    const auto body = json::parse(response->body);
    REQUIRE(body.at("semantic").at("providers").size() == 2);
    CHECK(body.at("semantic").at("default_provider") == "ollama");
    CHECK(body.at("semantic").at("providers").at(0).at("name") == "disabled");
    CHECK(body.at("semantic").at("providers").at(1).at("name") == "ollama");
}
