#include "ArpSender.h"

#include <EthLayer.h>
#include <ArpLayer.h>
#include <Packet.h>
#include <RawPacket.h>
#include <IpAddress.h>
#include <MacAddress.h>

#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>

struct ReplyCtx {
    pcpp::IPv4Address routerIp;
    std::string       routerMac;
    std::atomic<bool> found{false};
};

static void replyCb(u_char* user, const struct pcap_pkthdr* hdr, const u_char* data) {
    auto* ctx = reinterpret_cast<ReplyCtx*>(user);
    if (ctx->found) return;

    timeval ts = hdr->ts;
    pcpp::RawPacket rawPkt(data, (int)hdr->caplen, ts, false);
    pcpp::Packet parsed(&rawPkt);

    auto* arpLayer = parsed.getLayerOfType<pcpp::ArpLayer>();
    if (!arpLayer) return;
    if (ntohs(arpLayer->getArpHeader()->opcode) != pcpp::ARP_REPLY) return;

    if (arpLayer->getSenderIpAddr() == ctx->routerIp) {
        ctx->routerMac = arpLayer->getSenderMacAddress().toString();
        ctx->found = true;
    }
}

std::string ArpSender::findRouterMac(pcap_t* handle,
                               const std::string& myIp,
                               const std::string& myMac,
                               const std::string& routerIp,
                               int timeoutMs)
{
    std::cout << "\n[ArpSender] Отправляю ARP Request к роутеру " << routerIp << "...\n";

    pcpp::MacAddress  srcMac(myMac);
    pcpp::MacAddress  bcastMac("ff:ff:ff:ff:ff:ff");
    pcpp::MacAddress  zeroMac("00:00:00:00:00:00");
    pcpp::IPv4Address senderIp(myIp);
    pcpp::IPv4Address targetIp(routerIp);

    pcpp::EthLayer ethLayer(srcMac, bcastMac, PCPP_ETHERTYPE_ARP);
    pcpp::ArpLayer arpLayer(pcpp::ARP_REQUEST, srcMac, senderIp, zeroMac, targetIp);

    pcpp::Packet arpPacket(42);
    arpPacket.addLayer(&ethLayer);
    arpPacket.addLayer(&arpLayer);
    arpPacket.computeCalculateFields();

    // Запускаем приём в фоновом потоке
    ReplyCtx ctx;
    ctx.routerIp = targetIp;
    std::atomic<bool> stopCapture{false};

    std::thread captureThread([&]() {
        while (!stopCapture) {
            pcap_dispatch(handle, 50, replyCb, (u_char*)&ctx);
        }
    });

    // Отправляем пакет через pcap_inject
    const uint8_t* rawData = arpPacket.getRawPacket()->getRawData();
    int rawLen = arpPacket.getRawPacket()->getRawDataLen();
    if (pcap_inject(handle, rawData, (size_t)rawLen) == -1) {
        std::cerr << "[ArpSender] pcap_inject: " << pcap_geterr(handle) << "\n";
    } else {
        std::cout << "[ArpSender] Пакет отправлен. Ожидаю ARP Reply (таймаут "
                  << timeoutMs << " мс)...\n";
    }

    // Ждём ответ
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);
    while (!ctx.found && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    stopCapture = true;
    pcap_breakloop(handle);
    captureThread.join();

    if (ctx.found) {
        std::cout << "\u250c\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n";
        std::cout << "\u2502  Роутер " << routerIp << "\n";
        std::cout << "\u2502  MAC адрес: " << ctx.routerMac << "\n";
        std::cout << "\u2514\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n";
        return ctx.routerMac;
    } else {
        std::cout << "[ArpSender] Ответ не получен за " << timeoutMs << " мс.\n"
                  << "            Проверьте config.txt (ROUTER_IP, MY_IP, MY_MAC).\n";
        return "";
    }
}


// Контекст колбэка для ожидания ARP Reply


