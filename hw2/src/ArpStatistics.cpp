#include "ArpStatistics.h"

#include <EthLayer.h>
#include <ArpLayer.h>
#include <Packet.h>
#include <RawPacket.h>
#include <MacAddress.h>

#include <iostream>
#include <iomanip>
#include <atomic>
#include <thread>
#include <chrono>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>

struct StatsData {
    std::atomic<uint64_t> totalEthFrames{0};
    std::atomic<uint64_t> totalArpPackets{0};
    std::atomic<uint64_t> broadcastEthFrames{0};
    std::atomic<uint64_t> broadcastArpFrames{0};
    std::atomic<uint64_t> gratuitousArp{0};
    std::atomic<uint64_t> arpPairs{0};
    std::atomic<uint64_t> dataBytes{0};

    std::mutex            macMutex;
    std::set<std::string> uniqueMacs;

    std::mutex pairMutex;
    std::map<std::string, std::vector<std::string>> pendingRequests;

    std::string myMac;
    std::string routerMac;
};

static const std::string BROADCAST_MAC = "ff:ff:ff:ff:ff:ff";

static void statsCb(u_char* user, const struct pcap_pkthdr* hdr, const u_char* data) {
    auto* stats = reinterpret_cast<StatsData*>(user);

    timeval ts = hdr->ts;
    pcpp::RawPacket rawPkt(data, (int)hdr->caplen, ts, false);
    pcpp::Packet parsedPacket(&rawPkt);

    auto* ethLayer = parsedPacket.getLayerOfType<pcpp::EthLayer>();
    if (!ethLayer) return;

    stats->totalEthFrames++;

    auto toLower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(c));
        return s;
    };
    std::string srcMac = toLower(ethLayer->getSourceMac().toString());
    std::string dstMac = toLower(ethLayer->getDestMac().toString());

    {
        std::lock_guard<std::mutex> lk(stats->macMutex);
        if (srcMac != BROADCAST_MAC) stats->uniqueMacs.insert(srcMac);
        if (dstMac != BROADCAST_MAC) stats->uniqueMacs.insert(dstMac);
    }

    bool isBroadcast = (dstMac == BROADCAST_MAC);
    if (isBroadcast) stats->broadcastEthFrames++;

    auto* arpLayer = parsedPacket.getLayerOfType<pcpp::ArpLayer>();
    if (arpLayer) {
        stats->totalArpPackets++;
        if (isBroadcast) stats->broadcastArpFrames++;

        uint16_t op = ntohs(arpLayer->getArpHeader()->opcode);
        std::string senderIp = arpLayer->getSenderIpAddr().toString();
        std::string targetIp = arpLayer->getTargetIpAddr().toString();

        if (op == pcpp::ARP_REQUEST) {
            if (senderIp == targetIp) {
                stats->gratuitousArp++;
            } else {
                std::lock_guard<std::mutex> lk(stats->pairMutex);
                stats->pendingRequests[targetIp].push_back(senderIp);
            }
        } else if (op == pcpp::ARP_REPLY) {
            std::lock_guard<std::mutex> lk(stats->pairMutex);
            auto it = stats->pendingRequests.find(senderIp);
            if (it != stats->pendingRequests.end() && !it->second.empty()) {
                auto& vec = it->second;
                auto found = std::find(vec.begin(), vec.end(), targetIp);
                if (found != vec.end()) {
                    stats->arpPairs++;
                    vec.erase(found);
                }
            }
        }
    }

    {
        std::string myMacL     = stats->myMac;
        std::string routerMacL = stats->routerMac;
        for (auto& c : myMacL)     c = static_cast<char>(std::tolower(c));
        for (auto& c : routerMacL) c = static_cast<char>(std::tolower(c));

        bool isMyDevice = (srcMac == myMacL || dstMac == myMacL);
        bool isRouter   = (srcMac == routerMacL || dstMac == routerMacL);

        if (isMyDevice && isRouter && !routerMacL.empty())
            stats->dataBytes += hdr->caplen;
    }
}

void ArpStatistics::collect(pcap_t* handle,
                             const std::string& myMac,
                             const std::string& routerMac,
                             int durationSec)
{
    StatsData stats;
    stats.myMac     = myMac;
    stats.routerMac = routerMac;

    if (routerMac.empty()) {
        std::cout << "[ArpStatistics] ВНИМАНИЕ: MAC роутера не задан (статистика 8 = 0).\n"
                  << "                Сначала выполните команду 2.\n\n";
    }

    std::cout << "\n[ArpStatistics] Сбор статистики за " << durationSec << " сек...\n\n";

    std::atomic<bool> stopCapture{false};
    std::thread captureThread([&]() {
        while (!stopCapture) {
            pcap_dispatch(handle, 200, statsCb, (u_char*)&stats);
        }
    });

    for (int rem = durationSec; rem > 0; --rem) {
        std::cout << "\r  Осталось: " << std::setw(3) << rem << " сек"
                  << "  (кадров: " << stats.totalEthFrames.load() << ")"
                  << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "\n";

    stopCapture = true;
    pcap_breakloop(handle);
    captureThread.join();

    size_t uniqueMacCount = 0;
    {
        std::lock_guard<std::mutex> lk(stats.macMutex);
        uniqueMacCount = stats.uniqueMacs.size();
    }

    std::cout << "\n";
    std::cout << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n";
    std::cout << "\u2551        СТАТИСТИКА ЗА " << durationSec << " СЕК"
              << std::string(32 - std::to_string(durationSec).size(), ' ') << "\u2551\n";
    std::cout << "\u2560\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2563\n";
    std::cout << "\u2551  1. Всего Ethernet-фреймов:           "
              << std::setw(9) << stats.totalEthFrames   << "          \u2551\n";
    std::cout << "\u2551  2. Всего ARP пакетов:                "
              << std::setw(9) << stats.totalArpPackets  << "          \u2551\n";
    std::cout << "\u2551  3. Уникальных MAC-адресов:           "
              << std::setw(9) << uniqueMacCount         << "          \u2551\n";
    std::cout << "\u2551  4. Broadcast Ethernet-фреймов:       "
              << std::setw(9) << stats.broadcastEthFrames << "          \u2551\n";
    std::cout << "\u2551  5.   из них с протоколом ARP:        "
              << std::setw(9) << stats.broadcastArpFrames << "          \u2551\n";
    std::cout << "\u2551  6. Gratuitous ARP Requests:          "
              << std::setw(9) << stats.gratuitousArp    << "          \u2551\n";
    std::cout << "\u2551  7. Пар ARP targeted req/resp:        "
              << std::setw(9) << stats.arpPairs         << "          \u2551\n";
    std::cout << "\u2551  8. Байт между устройством и роутером:"
              << std::setw(9) << stats.dataBytes        << "          \u2551\n";
    std::cout << "\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n";

    if (uniqueMacCount > 0 && uniqueMacCount <= 20) {
        std::cout << "\n  Найденные MAC-адреса:\n";
        std::string myMacL    = stats.myMac;
        std::string routerMacL = stats.routerMac;
        for (auto& c : myMacL)     c = static_cast<char>(std::tolower(c));
        for (auto& c : routerMacL) c = static_cast<char>(std::tolower(c));
        std::lock_guard<std::mutex> lk(stats.macMutex);
        for (const auto& mac : stats.uniqueMacs) {
            std::cout << "    " << mac;
            if (mac == myMacL)    std::cout << "  <- это вы";
            if (!routerMacL.empty() && mac == routerMacL) std::cout << "  <- роутер";
            std::cout << "\n";
        }
    }
}


// Данные о незакрытом ARP request


