#include "Config.h"
#include "DnsCapture.h"
#include "DnsSender.h"

#include <pcap/pcap.h>
#include <iostream>
#include <string>

static void printHelp() {
    std::cout << "\n";
    std::cout << "===========================================================\n";
    std::cout << "       HW3: Утилита DNS (L7 / DNS Protocol)                \n";
    std::cout << "===========================================================\n";
    std::cout << "  Команды:                                                 \n";
    std::cout << "  1 - Захват всех DNS пакетов (promiscuous mode)             \n";
    std::cout << "  2 - Найти почтовый сервер для домена (MX запись)           \n";
    std::cout << "  3 - Опрос корневого и провайдерского DNS (задача 3)        \n";
    std::cout << "  h - Показать это меню снова                              \n";
    std::cout << "  0 - Выход                                                \n";
    std::cout << "===========================================================\n\n";
}

int main() {
    Config cfg;
    if (!cfg.load("../config.txt")) {
        std::cerr << "[main] Ошибка загрузки config.txt\n";
        return 1;
    }
    std::cout << "\nЗагружен config.txt:\n";
    cfg.print();

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
    
    // Устанавливаем BPF фильтр на порт 53 (DNS)
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, "udp port 53", 0, PCAP_NETMASK_UNKNOWN) == -1) {
        std::cerr << "pcap_compile failed\n";
        return 1;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        std::cerr << "pcap_setfilter failed\n";
        return 1;
    }

    std::cout << "Интерфейс " << cfg.iface << " открыт (фильтр UDP port 53).\n";
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
            // Задача 1: Захват DNS
            std::cout << "Нажмите Ctrl+C для остановки...\n";
            DnsCapture capture;
            capture.start(handle);
        } else if (cmd == "2") {
            // Задача 2: Поиск MX
            std::cout << "Введите доменное имя (например hse.ru): ";
            std::string domain;
            std::getline(std::cin, domain);
            if (!domain.empty()) {
                DnsSender sender(cfg);
                sender.findMxRecord(handle, domain);
            }
        } else if (cmd == "3") {
            // Задача 3: Корневые и провайдерские
            DnsSender sender(cfg);
            sender.runTask3(handle);
        } else {
            std::cout << "Неизвестная команда. Введите 'h' для помощи.\n";
        }
    }

    pcap_close(handle);
    return 0;
}
