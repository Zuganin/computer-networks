#pragma once
#include <pcap/pcap.h>

// ArpCapture - Zadacha 1
class ArpCapture {
public:
    void start(pcap_t* handle);
};
