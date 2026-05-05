#pragma once
#include <string>

struct Config {
    std::string myIp;
    std::string myMac;
    std::string routerIp;
    std::string routerMac;
    std::string iface;
    std::string rootDnsIp;
    std::string ispDnsIp;

    bool load(const std::string& filename);
    void print() const;
};
