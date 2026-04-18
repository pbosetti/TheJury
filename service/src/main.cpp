#include <iostream>

#include <httplib.h>

#include "ppa/api/Handlers.hpp"

int main() {
    httplib::Server server;
    ppa::api::register_routes(server);

    constexpr auto* host = "127.0.0.1";
    constexpr auto port = 6464;

    std::cout << "ppa_service listening on http://" << host << ':' << port << '\n';
    if (!server.listen(host, port)) {
        std::cerr << "failed to bind http server" << '\n';
        return 1;
    }

    return 0;
}
