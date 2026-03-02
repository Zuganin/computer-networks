#include "Config.h"
#include "ArpCapture.h"
#include "ArpSender.h"
#include "ArpStatistics.h"

#include <PcapLiveDeviceList.h>
#include <PcapLiveDevice.h>
#include <IpAddress.h>

#include <iostream>
#include <string>
#include <vector>
#include <limits>

// Вывод списка команд

static void printHelp() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║       HW2: Утилита ARP (Protocol ARP / Data Link)       ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout << "║  Команды:                                                ║\n";
    std::cout << "║  1 — Захват всех ARP пакетов (promiscuous mode)          ║\n";
    std::cout << "║  2 — Узнать MAC адрес роутера (ARP Request/Reply)        ║\n";
    std::cout << "║  3 — Собрать и вывести статистику за N секунд            ║\n";
    std::cout << "║  h — Показать это меню снова                             ║\n";
    std::cout << "║  0 — Выход                                               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

// Поиск интерфейса по IP адресу из конфига

static pcpp::PcapLiveDevice* chooseDevice(const std::string& iface) {
    // Открываем напрямую по имени — НЕ итерируем все интерфейсы.
    // Итерация падает с исключением на IPv6-only интерфейсах (utun*, lo0 и т.д.).
    pcpp::PcapLiveDevice* dev =
        pcpp::PcapLiveDeviceList::getInstance().getDeviceByName(iface);

    if (!dev) {
        std::cerr << "[main] Интерфейс '" << iface << "' не найден.\n"
                  << "       Проверьте INTERFACE в config.txt.\n"
                  << "       Доступные интерфейсы: ifconfig | grep '^[a-z]'\n";
        return nullptr;
    }

    std::cout << "Найден интерфейс: " << dev->getName() << "\n";
    return dev;
}

// Точка входа

int main() {
    // Загружаем конфиг
    Config cfg;
    if (!cfg.load("config.txt")) {
        std::cerr << "[main] Ошибка загрузки config.txt\n"
                  << "       Заполните MY_IP, MY_MAC, ROUTER_IP и запустите снова.\n";
        return 1;
    }

    std::cout << "\nЗагружен config.txt:\n";
    cfg.print();

    // Находим интерфейс по IP из конфига
    pcpp::PcapLiveDevice* device = chooseDevice(cfg.iface);
    if (!device) return 1;

    std::cout << "Интерфейс: " << device->getName() << "\n";

    // Открываем устройство в promiscuous режиме
    pcpp::PcapLiveDevice::DeviceConfiguration config;
    config.mode = pcpp::PcapLiveDevice::Promiscuous;

    if (!device->open(config)) {
        std::cerr << "[main] Не удалось открыть интерфейс.\n"
                  << "       Попробуйте запустить с sudo.\n";
        return 1;
    }

    std::cout << "Интерфейс открыт в PROMISCUOUS режиме.\n";

    // Сохраняем MAC роутера (заполняется в задаче 2)
    std::string routerMacCached;

    // Главный цикл меню
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
            // Задача 1: Захват ARP пакетов
            ArpCapture capture;
            capture.start(device);

        } else if (cmd == "2") {
            // Задача 2: Узнать MAC роутера
            ArpSender sender;
            sender.findRouterMac(device, cfg.myIp, cfg.myMac, cfg.routerIp);

            // Сохраняем MAC для задачи 3
            // Повторно захватываем, прокидывая контекст через временный захват
            // (В рамках учебного задания: просим пользователя ввести полученный MAC)
            std::cout << "\nДля статистики введите MAC роутера, который вы получили\n"
                      << "(или оставьте пустым, чтобы пропустить): ";
            std::string input;
            std::getline(std::cin, input);
            if (!input.empty()) {
                routerMacCached = input;
                std::cout << "MAC роутера сохранён: " << routerMacCached << "\n";
            }

        } else if (cmd == "3") {
            // Задача 3: Сбор статистики
            std::cout << "Введите время сбора в секундах: ";
            std::string secStr;
            std::getline(std::cin, secStr);

            int seconds = 0;
            try {
                seconds = std::stoi(secStr);
            } catch (...) {
                std::cerr << "Неверное число секунд.\n";
                continue;
            }

            if (seconds <= 0 || seconds > 3600) {
                std::cerr << "Введите число от 1 до 3600.\n";
                continue;
            }

            ArpStatistics stats;
            stats.collect(device, cfg.myMac, routerMacCached, seconds);

        } else {
            std::cout << "Неизвестная команда. Введите 'h' для помощи.\n";
        }
    }

    device->close();
    return 0;
}
