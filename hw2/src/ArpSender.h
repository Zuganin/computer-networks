#pragma once
#include <string>

namespace pcpp { class PcapLiveDevice; }

/**
 * ArpSender — Задача 2
 * Отправляет ARP Request к роутеру для определения его MAC-адреса.
 * Ожидаем ARP Reply от роутера — в нём будет его MAC.
 */

class ArpSender {
public:
    /**
     * Отправляет ARP Request к routerIp и ждёт ответ.
     * Выводит MAC роутера в консоль.
     */
    void findRouterMac(pcpp::PcapLiveDevice* device,
                       const std::string& myIp,
                       const std::string& myMac,
                       const std::string& routerIp,
                       int timeoutMs = 3000);
};
