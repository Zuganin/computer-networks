#include "client.h"
#include <iostream>
#include <string>
#include <yaml-cpp/yaml.h>

static const std::string CONFIG_PATH =
    "/Users/vadimzenin/ДЗшки/computer-network/hw4/src/common/config.yaml";

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           👤 MyDrive Client - Version 1.0                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    YAML::Node config;
    try {
        config = YAML::LoadFile(CONFIG_PATH);
    } catch (const std::exception& e) {
        std::cerr << "Ошибка чтения конфига: " << e.what() << "\n";
        return 1;
    }

    int total_clients = static_cast<int>(config["clients"].size());

    int client_number = 0;
    std::cout << "Доступные клиенты: 1 .. " << total_clients << "\n";
    std::cout << "Введите номер клиента: ";

    std::string input;
    std::getline(std::cin, input);

    try {
        client_number = std::stoi(input);
    } catch (...) {
        std::cerr << "Ошибка: введите целое число.\n";
        return 1;
    }

    if (client_number < 1 || client_number > total_clients) {
        std::cerr << "\nКлиент " << client_number << " не найден в конфиге.\n"
                  << "Сейчас доступны клиенты: 1 .. " << total_clients << ".\n"
                  << "Добавьте нового клиента в секцию 'clients' файла config.yaml "
                  << "и повторите запуск.\n";
        return 1;
    }

    int client_index = client_number -1 ;

    try {
        Client client(CONFIG_PATH, client_index);
        std::cout << "   Client ID: " << client.GetId() << "\n\n";
        client.StartSync();
    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
