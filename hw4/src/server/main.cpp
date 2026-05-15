#include <iostream>
#include "server.h"

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           🚀 MyDrive Server - Version 1.0                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    Server server = Server("../src/common/config.yaml");
    server.Start();

    return 0;
}
