#include "client.h"
#include <iostream>

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           👤 MyDrive Client - Version 1.0                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    try {
        Client client("/Users/vadimzenin/ДЗшки/computer-network/hw4/src/common/config.yaml");
        std::cout << "   Client ID: " << client.GetId() << "\n\n";
        client.StartSync();

    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
