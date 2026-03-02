#include "ArpCapture.h"

#include <EthLayer.h>
#include <ArpLayer.h>
#include <Packet.h>
#include <RawPacket.h>

#include <iostream>
#include <iomanip>
#include <atomic>
#include <thread>

struct CapCtx {
    std::atomic<bool> running{true};
};

static void printArpPacket(const pcpp::ArpLayer* arp, const pcpp::EthLayer* eth) {
    const pcpp::arphdr* hdr = arp->getArpHeader();
    uint16_t op    = ntohs(hdr->opcode);
    uint16_t htype = ntohs(hdr->hardwareType);
    uint16_t ptype = ntohs(hdr->protocolType);

    std::string opStr = (op == pcpp::ARP_REQUEST) ? "REQUEST" :
                        (op == pcpp::ARP_REPLY)   ? "REPLY"   :
                                                    "UNKNOWN(" + std::to_string(op) + ")";

    std::cout << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";
    std::cout << "\u2502  ARP " << opStr << "\n";
    std::cout << "\u2502  Ethernet:  " << eth->getSourceMac() << "  ->  "
                                       << eth->getDestMac()  << "\n";
    std::cout << "\u2502  HW Type:   " << htype << " (";
    if      (htype == 0x0001) std::cout << "Ethernet)";
    else if (htype == 0x0006) std::cout << "IEEE 802)";
    else                      std::cout << "other)";
    std::cout << "\n";
    std::cout << "\u2502  Proto:     0x" << std::hex << std::uppercase
              << std::setw(4) << std::setfill('0') << ptype << std::dec;
    if (ptype == 0x0800) std::cout << " (IPv4)";
    std::cout << "\n";
    std::cout << "\u2502  Sender:    " << arp->getSenderMacAddress()
              << "  /  " << arp->getSenderIpAddr() << "\n";
    std::cout << "\u2502  Target:    " << arp->getTargetMacAddress()
              << "  /  " << arp->getTargetIpAddr() << "\n";
    if (op == pcpp::ARP_REQUEST &&
        arp->getSenderIpAddr() == arp->getTargetIpAddr()) {
        std::cout << "\u2502  *** GRATUITOUS ARP ***\n";
    }
    std::cout << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";
}

static void pcapCb(u_char* user, const struct pcap_pkthdr* hdr, const u_char* data) {
    auto* ctx = reinterpret_cast<CapCtx*>(user);
    if (!ctx->running) return;

    timeval ts = hdr->ts;
    pcpp::RawPacket rawPkt(data, (int)hdr->caplen, ts, false);
    pcpp::Packet parsed(&rawPkt);

    auto* arpLayer = parsed.getLayerOfType<pcpp::ArpLayer>();
    if (!arpLayer) return;
    auto* ethLayer = parsed.getLayerOfType<pcpp::EthLayer>();
    if (!ethLayer) return;

    printArpPacket(arpLayer, ethLayer);
}

void ArpCapture::start(pcap_t* handle) {
    CapCtx ctx;

    std::cout << "\n[ArpCapture] Захват ARP пакетов запущен.\n"
              << "             Нажмите Enter для остановки...\n\n";

    std::thread captureThread([&]() {
        while (ctx.running) {
            pcap_dispatch(handle, 100, pcapCb, (u_char*)&ctx);
        }
    });

    std::cin.get();

    ctx.running = false;
    pcap_breakloop(handle);
    captureThread.join();

    std::cout << "[ArpCapture] Захват остановлен.\n";
}


// Данные, передаваемые в колбэк


