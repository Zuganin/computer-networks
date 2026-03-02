#include "ArpCapture.h"

#include <PcapLiveDevice.h>
#include <EthLayer.h>
#include <ArpLayer.h>
#include <Packet.h>

#include <iostream>
#include <iomanip>
#include <atomic>
#include <thread>
#include <chrono>

// Данные, передаваемые в колбэк
struct CaptureContext {
    std::atomic<bool> running{true};
};

//  Вспомогательная функция: красивый вывод ARP пакета 

static void printArpPacket(const pcpp::ArpLayer* arp, const pcpp::EthLayer* eth) {
    const pcpp::arphdr* hdr = arp->getArpHeader();

    uint16_t op    = ntohs(hdr->opcode);
    uint16_t htype = ntohs(hdr->hardwareType);
    uint16_t ptype = ntohs(hdr->protocolType);

    std::string opStr = (op == pcpp::ARP_REQUEST) ? "REQUEST" :
                        (op == pcpp::ARP_REPLY)   ? "REPLY"   : "UNKNOWN(" + std::to_string(op) + ")";

    std::cout << "┌─────────────────────────────────────────────────────┐\n";
    std::cout << "│  ARP " << opStr << "\n";
    std::cout << "│  Ethernet:  " << eth->getSourceMac()  << "  →  "
                                   << eth->getDestMac()   << "\n";

    std::cout << "│  HW Type:   " << htype << " (";
    if      (htype == 0x0001) std::cout << "Ethernet)";
    else if (htype == 0x0006) std::cout << "IEEE 802)";
    else                      std::cout << "other)";
    std::cout << "\n";

    std::cout << "│  Proto:     0x" << std::hex << std::uppercase
              << std::setw(4) << std::setfill('0') << ptype
              << std::dec;
    if (ptype == 0x0800) std::cout << " (IPv4)";
    std::cout << "\n";

    std::cout << "│  Sender:    " << arp->getSenderMacAddress()
              << "  /  " << arp->getSenderIpAddr() << "\n";
    std::cout << "│  Target:    " << arp->getTargetMacAddress()
              << "  /  " << arp->getTargetIpAddr() << "\n";

    // Определяем Gratuitous ARP
    if (op == pcpp::ARP_REQUEST &&
        arp->getSenderIpAddr() == arp->getTargetIpAddr()) {
        std::cout << "│  *** GRATUITOUS ARP ***\n";
    }

    std::cout << "└─────────────────────────────────────────────────────┘\n";
}

// Колбэк захвата

static void onPacketArrives(pcpp::RawPacket* rawPacket,
                            pcpp::PcapLiveDevice* /*dev*/,
                            void* cookie)
{
    auto* ctx = static_cast<CaptureContext*>(cookie);
    if (!ctx->running) return;

    pcpp::Packet parsedPacket(rawPacket);

    auto* arpLayer = parsedPacket.getLayerOfType<pcpp::ArpLayer>();
    if (!arpLayer) return;  // не ARP, пропускаем

    auto* ethLayer = parsedPacket.getLayerOfType<pcpp::EthLayer>();
    if (!ethLayer) return;

    printArpPacket(arpLayer, ethLayer);
}

// Публичный метод

void ArpCapture::start(pcpp::PcapLiveDevice* device) {
    CaptureContext ctx;

    std::cout << "\n[ArpCapture] Захват ARP пакетов (promiscuous mode).\n"
              << "             Нажмите Enter для остановки...\n\n";

    if (!device->startCapture(onPacketArrives, &ctx)) {
        std::cerr << "[ArpCapture] Не удалось запустить захват.\n";
        return;
    }

    // Ждём нажатия Enter
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();

    ctx.running = false;
    device->stopCapture();

    std::cout << "[ArpCapture] Захват остановлен.\n";
}
