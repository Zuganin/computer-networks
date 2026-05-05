#pragma once
#include "Config.h"
#include <pcap/pcap.h>
#include <string>

class DnsSender {
public:
    DnsSender(const Config& cfg);
    void findMxRecord(pcap_t* handle, const std::string& domain);
    void runTask3(pcap_t* handle);

private:
    void findIpForMx(pcap_t* handle, const std::string& mxServer, const std::string& originalDomain);
    Config config;
    void sendDnsQuery(pcap_t* handle, const std::string& domain, uint16_t type, const std::string& dnsServerIp);
};
