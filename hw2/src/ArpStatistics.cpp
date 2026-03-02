#include "ArpStatistics.h"

#include <PcapLiveDevice.h>
#include <EthLayer.h>
#include <ArpLayer.h>
#include <Packet.h>
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

// Данные о незакрытом ARP request

struct ArpRequest {
    std::string senderIp;
    std::string targetIp;
};

// Аккумулятор статистики 
struct StatsData {
    // Счётчики
    std::atomic<uint64_t> totalEthFrames{0};
    std::atomic<uint64_t> totalArpPackets{0};
    std::atomic<uint64_t> broadcastEthFrames{0};
    std::atomic<uint64_t> broadcastArpFrames{0};
    std::atomic<uint64_t> gratuitousArp{0};
    std::atomic<uint64_t> arpPairs{0};
    std::atomic<uint64_t> dataBytes{0};

    // Уникальные MAC 
    std::mutex macMutex;
    std::set<std::string>  uniqueMacs;

    // ARP Requests, ожидающие совпадающего Reply
    // Ключ: targetIp - список pending requests (senderIp)
    std::mutex pairMutex;
    std::map<std::string, std::vector<std::string>> pendingRequests;

    // MAC адреса нашего устройства и роутера
    std::string myMac;
    std::string routerMac;
};

static const std::string BROADCAST_MAC = "ff:ff:ff:ff:ff:ff";

// Колбэк

static void onPacketStats(pcpp::RawPacket* rawPacket,
                          pcpp::PcapLiveDevice* /*dev*/,
                          void* cookie)
{
    auto* stats = static_cast<StatsData*>(cookie);
    pcpp::Packet parsedPacket(rawPacket);

    auto* ethLayer = parsedPacket.getLayerOfType<pcpp::EthLayer>();
    if (!ethLayer) return;

    // Счётчик Ethernet-фреймов
    stats->totalEthFrames++;

    std::string srcMac = ethLayer->getSourceMac().toString();
    std::string dstMac = ethLayer->getDestMac().toString();

    // Приводим к нижнему регистру для единообразия
    auto toLower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(c));
        return s;
    };
    srcMac = toLower(srcMac);
    dstMac = toLower(dstMac);

    // Уникальные MAC адреса
    {
        std::lock_guard<std::mutex> lk(stats->macMutex);
        if (srcMac != BROADCAST_MAC) stats->uniqueMacs.insert(srcMac);
        if (dstMac != BROADCAST_MAC) stats->uniqueMacs.insert(dstMac);
    }

    // Broadcast Ethernet
    bool isBroadcast = (dstMac == BROADCAST_MAC);
    if (isBroadcast) stats->broadcastEthFrames++;

    // Статистика ARP
    auto* arpLayer = parsedPacket.getLayerOfType<pcpp::ArpLayer>();
    if (arpLayer) {
        stats->totalArpPackets++;

        if (isBroadcast) stats->broadcastArpFrames++;

        uint16_t op = ntohs(arpLayer->getArpHeader()->opcode);
        std::string senderIp = arpLayer->getSenderIpAddr().toString();
        std::string targetIp = arpLayer->getTargetIpAddr().toString();

        if (op == pcpp::ARP_REQUEST) {
            // Gratuitous ARP (Sender IP == Target IP)
            if (senderIp == targetIp) {
                stats->gratuitousArp++;
            }
            // Сохраняем request для сопоставления
            else {
                std::lock_guard<std::mutex> lk(stats->pairMutex);
                stats->pendingRequests[targetIp].push_back(senderIp);
            }

        } else if (op == pcpp::ARP_REPLY) {
            // Ищем совпадающий request
            // Reply: senderIp = тот, кого спрашивали; targetIp = тот, кто спрашивал
            std::lock_guard<std::mutex> lk(stats->pairMutex);
            auto it = stats->pendingRequests.find(senderIp);
            if (it != stats->pendingRequests.end() && !it->second.empty()) {
                // Ищем в списке совпадающий originalSender == targetIp ответа
                auto& vec = it->second;
                auto found = std::find(vec.begin(), vec.end(), targetIp);
                if (found != vec.end()) {
                    stats->arpPairs++;
                    vec.erase(found);  // удаляем, чтобы не считать дважды
                }
            }
        }
    }

    // Объём данных между нашим устройством и роутером
    // Считаем длину полного Ethernet фрейма (включая padding в Data поле)
    {
        std::string myMacLower    = stats->myMac;
        std::string routerMacLower = stats->routerMac;
        for (auto& c : myMacLower)     c = static_cast<char>(std::tolower(c));
        for (auto& c : routerMacLower) c = static_cast<char>(std::tolower(c));

        bool isMyDevice = (srcMac == myMacLower || dstMac == myMacLower);
        bool isRouter   = (srcMac == routerMacLower || dstMac == routerMacLower);

        if (isMyDevice && isRouter && !routerMacLower.empty()) {
            uint32_t frameLen = rawPacket->getRawDataLen();
            stats->dataBytes += frameLen;
        }
    }
}

// Публичный метод

void ArpStatistics::collect(pcpp::PcapLiveDevice* device,
                             const std::string& myMac,
                             const std::string& routerMac,
                             int durationSec)
{
    StatsData stats;
    stats.myMac     = myMac;
    stats.routerMac = routerMac;

    if (routerMac.empty()) {
        std::cout << "[ArpStatistics] ВНИМАНИЕ: MAC роутера не задан.\n"
                  << "                Статистика объёма данных будет 0.\n"
                  << "                Сначала выполните Задачу 2.\n\n";
    }

    std::cout << "\n[ArpStatistics] Сбор статистики за " << durationSec << " сек...\n\n";

    if (!device->startCapture(onPacketStats, &stats)) {
        std::cerr << "[ArpStatistics] Ошибка запуска захвата.\n";
        return;
    }

    // Показываем обратный отсчёт
    for (int remaining = durationSec; remaining > 0; --remaining) {
        std::cout << "\r  Осталось: " << std::setw(3) << remaining << " сек"
                  << "  (кадров: " << stats.totalEthFrames.load() << ")"
                  << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "\n";

    device->stopCapture();

    // Вывод результатов
    size_t uniqueMacCount = 0;
    {
        std::lock_guard<std::mutex> lk(stats.macMutex);
        uniqueMacCount = stats.uniqueMacs.size();
    }

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║        СТАТИСТИКА ЗА " << durationSec << " СЕК"
              << std::string(32 - std::to_string(durationSec).size(), ' ') << "║\n";
    std::cout << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout << "║  1. Всего Ethernet-фреймов:           "
              << std::setw(9) << stats.totalEthFrames << "          ║\n";
    std::cout << "║  2. Всего ARP пакетов:                "
              << std::setw(9) << stats.totalArpPackets << "          ║\n";
    std::cout << "║  3. Уникальных MAC-адресов:           "
              << std::setw(9) << uniqueMacCount << "          ║\n";
    std::cout << "║  4. Broadcast Ethernet-фреймов:       "
              << std::setw(9) << stats.broadcastEthFrames << "          ║\n";
    std::cout << "║  5.   из них с протоколом ARP:        "
              << std::setw(9) << stats.broadcastArpFrames << "          ║\n";
    std::cout << "║  6. Gratuitous ARP Requests:          "
              << std::setw(9) << stats.gratuitousArp << "          ║\n";
    std::cout << "║  7. Пар ARP targeted req/resp:        "
              << std::setw(9) << stats.arpPairs << "          ║\n";
    std::cout << "║  8. Байт между устройством и роутером:"
              << std::setw(9) << stats.dataBytes << "          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";

    // Список уникальных MAC (полезно для отчёта)
    if (uniqueMacCount > 0 && uniqueMacCount <= 20) {
        std::cout << "\n  Найденные MAC-адреса:\n";

        std::string myMacLow    = stats.myMac;
        std::string routerMacLow = stats.routerMac;
        for (auto& c : myMacLow)     c = static_cast<char>(std::tolower(c));
        for (auto& c : routerMacLow) c = static_cast<char>(std::tolower(c));

        std::lock_guard<std::mutex> lk(stats.macMutex);
        for (const auto& mac : stats.uniqueMacs) {
            std::cout << "    " << mac;
            if (mac == myMacLow)     std::cout << "  ← это вы";
            if (mac == routerMacLow && !routerMacLow.empty())
                std::cout << "  ← роутер";
            std::cout << "\n";
        }
    }
}
