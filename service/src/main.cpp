#include <iostream>

#include <httplib.h>

#include "ppa/api/Handlers.hpp"
#include "ppa/config/ServiceConfig.hpp"

int main() {
    try {
        const auto executable_path = ppa::current_executable_path();
        const auto loaded_config = ppa::load_service_config(executable_path.parent_path());
        auto service = ppa::CritiqueService(loaded_config.config);
        auto server = httplib::Server{};
        ppa::api::register_routes(server, service, loaded_config.resolved_path);

        constexpr auto* host = "127.0.0.1";
        constexpr auto port = 6464;

        std::cout << "ppa_service listening on http://" << host << ':' << port << '\n';
        if (!server.listen(host, port)) {
            std::cerr << "failed to bind http server" << '\n';
            return 1;
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
