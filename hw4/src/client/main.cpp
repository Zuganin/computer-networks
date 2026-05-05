#include "client.h"
#include <iostream>

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           👤 MyDrive Client - Version 1.0                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    try {
        Client client("/Users/vadimzenin/ДЗшки/computer-network/hw4/src/common/config.yaml");
        std::cout << "✅ Client is ready!\n";
        std::cout << "   Client ID: " << client.GetId() << "\n\n";
        client.ConnectToServer();

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\n";
        return 1;
    }
    while(true){}

    return 0;
}
