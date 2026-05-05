#pragma once
#include <pcap/pcap.h>

class DnsCapture {
public:
    void start(pcap_t* handle);
};
