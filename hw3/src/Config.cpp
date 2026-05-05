#include "Config.h"
#include <fstream>
#include <iostream>

bool Config::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            if (val.back() == '\r') val.pop_back(); // Обработка CRLF
            
            if (key == "MY_IP") myIp = val;
            else if (key == "MY_MAC") myMac = val;
            else if (key == "ROUTER_IP") routerIp = val;
            else if (key == "ROUTER_MAC") routerMac = val;
            else if (key == "INTERFACE") iface = val;
            else if (key == "ROOT_DNS_IP") rootDnsIp = val;
            else if (key == "ISP_DNS_IP") ispDnsIp = val;
        }
    }
    return true;
}

void Config::print() const {
    std::cout << "  Interface: " << iface << "\n"
              << "  My IP:     " << myIp << "\n"
              << "  My MAC:    " << myMac << "\n"
              << "  Router MAC:" << routerMac << "\n"
              << "  Root DNS:  " << rootDnsIp << "\n"
              << "  ISP DNS:   " << ispDnsIp << "\n\n";
}
