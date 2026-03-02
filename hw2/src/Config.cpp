#include "Config.h"
#include <fstream>
#include <iostream>
#include <sstream>

bool Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Config] Не удалось открыть " << path << "\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Пропускаем комментарии и пустые строки
        if (line.empty() || line[0] == '#') continue;

        auto sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key   = line.substr(0, sep);
        std::string value = line.substr(sep + 1);

        // Убираем пробелы по краям
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\r')) s.erase(s.begin());
            while (!s.empty() && (s.back()  == ' ' || s.back()  == '\r')) s.pop_back();
        };
        trim(key);
        trim(value);

        if      (key == "MY_IP")     myIp     = value;
        else if (key == "MY_MAC")    myMac    = value;
        else if (key == "ROUTER_IP") routerIp = value;
        else if (key == "INTERFACE") iface    = value;
    }

    if (myIp.empty() || myMac.empty() || routerIp.empty() || iface.empty()) {
        std::cerr << "[Config] config.txt неполный. Нужны MY_IP, MY_MAC, ROUTER_IP, INTERFACE\n";
        return false;
    }
    return true;
}

void Config::print() const {
    std::cout << "  MY_IP     = " << myIp     << "\n"
              << "  MY_MAC    = " << myMac    << "\n"
              << "  ROUTER_IP = " << routerIp << "\n"
              << "  INTERFACE = " << iface    << "\n";
}
