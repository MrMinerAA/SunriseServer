#include "server/server.h"

#include <iostream>

int main() {
    std::cout << "SunriseServer starting...\n";

    sunrise::multiplayer::Server server;

    if (!server.initialize(30975)) {
        std::cerr << "Failed to initialize server.\n";
        return 1;
    }

    server.run();

    return 0;
}