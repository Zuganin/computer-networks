#include "ArpSender.h"

#include <PcapLiveDevice.h>
#include <EthLayer.h>
#include <ArpLayer.h>
#include <Packet.h>
#include <IpAddress.h>
#include <MacAddress.h>

#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <string>

// Контекст колбэка для ожидания ARP Reply

struct ReplyContext {
    pcpp::IPv4Address    routerIp;
    std::string          routerMac;      // результат (пустой пока не получен)
    std::atomic<bool>    found{false};
};

// Колбэк: ищет ARP Reply от роутера 

static void onReplyArrives(pcpp::RawPacket* rawPacket,
                           pcpp::PcapLiveDevice* /*dev*/,
                           void* cookie)
{
    auto* ctx = static_cast<ReplyContext*>(cookie);
    if (ctx->found) return;

    pcpp::Packet parsedPacket(rawPacket);

    auto* arpLayer = parsedPacket.getLayerOfType<pcpp::ArpLayer>();
    if (!arpLayer) return;

    // Нас интересует только ARP_REPLY
    if (ntohs(arpLayer->getArpHeader()->opcode) != pcpp::ARP_REPLY) return;

    // Проверяем: отправитель ответа — именно наш роутер?
    if (arpLayer->getSenderIpAddr() == ctx->routerIp) {
        ctx->routerMac = arpLayer->getSenderMacAddress().toString();
        ctx->found = true;
    }
}

// Публичный метод 

void ArpSender::findRouterMac(pcpp::PcapLiveDevice* device,
                               const std::string& myIp,
                               const std::string& myMac,
                               const std::string& routerIp,
                               int timeoutMs)
{
    std::cout << "\n[ArpSender] Отправляю ARP Request к роутеру " << routerIp << "...\n";

    // Собираем Ethernet + ARP пакет 
    pcpp::MacAddress  srcMac(myMac);
    pcpp::MacAddress  bcastMac("ff:ff:ff:ff:ff:ff");
    pcpp::MacAddress  zeroMac("00:00:00:00:00:00");
    pcpp::IPv4Address senderIp(myIp);
    pcpp::IPv4Address targetIp(routerIp);

    // Ethernet frame
    pcpp::EthLayer ethLayer(srcMac, bcastMac, PCPP_ETHERTYPE_ARP);

    // ARP Request — новый конструктор: (opCode, senderMAC, senderIP, targetMAC, targetIP)
    pcpp::ArpLayer arpLayer(pcpp::ARP_REQUEST,
                             srcMac,
                             senderIp,
                             zeroMac,
                             targetIp);

    // Собираем пакет (42 байта: 14 Ethernet + 28 ARP)
    pcpp::Packet arpPacket(42);
    arpPacket.addLayer(&ethLayer);
    arpPacket.addLayer(&arpLayer);
    arpPacket.computeCalculateFields();

    // Запускаем прослушивание ответов 
    ReplyContext ctx;
    ctx.routerIp = targetIp;

    if (!device->startCapture(onReplyArrives, &ctx)) {
        std::cerr << "[ArpSender] Ошибка запуска захвата.\n";
        return;
    }

    // Отправляем пакет 
    if (!device->sendPacket(&arpPacket)) {
        std::cerr << "[ArpSender] Ошибка отправки пакета.\n";
        device->stopCapture();
        return;
    }

    std::cout << "[ArpSender] Пакет отправлен. Ожидаю ARP Reply (таймаут "
              << timeoutMs << " мс)...\n";

    // Ждём ответ (polling с таймаутом)
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);

    while (!ctx.found && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    device->stopCapture();

    // Выводим результат 
    if (ctx.found) {
        std::cout << "┌──────────────────────────────────────────────┐\n";
        std::cout << "│  Роутер " << routerIp << "\n";
        std::cout << "│  MAC адрес: " << ctx.routerMac << "\n";
        std::cout << "└──────────────────────────────────────────────┘\n";
    } else {
        std::cout << "[ArpSender] Ответ не получен за " << timeoutMs << " мс.\n"
                  << "            Проверьте config.txt (ROUTER_IP, MY_IP, MY_MAC).\n";
    }
}
