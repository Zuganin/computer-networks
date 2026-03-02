#include "Config.h"
#include "ArpCapture.h"
#include "ArpSender.h"
#include "ArpStatistics.h"

#include <pcap/pcap.h>

#include <iostream>
#include <string>

static void printHelp() {
    std::cout << "\n";
    std::cout << "\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557\n";
    std::cout << "\u2551       HW2: Утилита ARP (Protocol ARP / Data Link)        \u2551\n";
    std::cout << "\u2560\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2563\n";
    std::cout << "\u2551  Команды:                                                \u2551\n";
    std::cout << "\u2551  1 — Захват всех ARP пакетов (promiscuous mode)          \u2551\n";
    std::cout << "\u2551  2 — Узнать MAC адрес роутера (ARP Request/Reply)        \u2551\n";
    std::cout << "\u2551  3 — Собрать и вывести статистику за N секунд            \u2551\n";
    std::cout << "\u2551  h — Показать это меню снова                             \u2551\n";
    std::cout << "\u2551  0 — Выход                                               \u2551\n";
    std::cout << "\u255a\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255d\n";
    std::cout << "\n";
}

int main() {
    Config cfg;
    if (!cfg.load("config.txt")) {
        std::cerr << "[main] Ошибка загрузки config.txt\n";
        return 1;
    }
    std::cout << "\nЗагружен config.txt:\n";
    cfg.print();

    // Открываем интерфейс напрямую через libpcap
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t* handle = pcap_open_live(
        cfg.iface.c_str(),
        65535,   // snaplen
        1,       // promiscuous
        100,     // read timeout ms
        errbuf
    );
    if (!handle) {
        std::cerr << "[main] pcap_open_live: " << errbuf << "\n"
                  << "       Запустите с sudo. Интерфейс: " << cfg.iface << "\n";
        return 1;
    }
    std::cout << "Интерфейс " << cfg.iface << " открыт в PROMISCUOUS режиме.\n";

    std::string routerMacCached;

    printHelp();

    std::string cmd;
    while (true) {
        std::cout << "Введите команду: ";
        std::getline(std::cin, cmd);
        if (cmd.empty()) continue;

        if (cmd == "0") {
            std::cout << "Выход.\n";
            break;

        } else if (cmd == "h" || cmd == "H") {
            printHelp();

        } else if (cmd == "1") {
            ArpCapture capture;
            capture.start(handle);

        } else if (cmd == "2") {
            ArpSender sender;
            std::string mac = sender.findRouterMac(handle, cfg.myIp, cfg.myMac, cfg.routerIp);
            if (!mac.empty()) {
                routerMacCached = mac;
                std::cout << "MAC роутера сохранён для задачи 3: " << routerMacCached << "\n";
            }

        } else if (cmd == "3") {
            if (routerMacCached.empty()) {
                std::cout << "[Совет] Сначала выполните команду 2, чтобы получить MAC роутера.\n";
            }
            std::cout << "Введите время сбора в секундах: ";
            std::string secStr;
            std::getline(std::cin, secStr);
            int seconds = 0;
            try { seconds = std::stoi(secStr); } catch (...) {}
            if (seconds <= 0 || seconds > 3600) {
                std::cerr << "Введите число от 1 до 3600.\n";
                continue;
            }
            ArpStatistics stats;
            stats.collect(handle, cfg.myMac, routerMacCached, seconds);

        } else {
            std::cout << "Неизвестная команда. Введите 'h' для помощи.\n";
        }
    }

    pcap_close(handle);
    return 0;
}


