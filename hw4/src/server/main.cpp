#include <iostream>
#include "server.h"

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           🚀 MyDrive Server - Version 1.0                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    Server server = Server("/Users/vadimzenin/ДЗшки/computer-network/hw4/src/common/config.yaml");
    server.Start();

    return 0;
}
