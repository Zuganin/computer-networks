#pragma once
#include <string>
#include <cstdint>

namespace pcpp { class PcapLiveDevice; }

/**
 * ArpStatistics — Задача 3
 * Собирает и выводит 8 показателей за заданное пользователем время:
 *
 *  1. Всего Ethernet-фреймов
 *  2. Всего ARP пакетов
 *  3. Уникальных MAC-адресов
 *  4. Broadcast Ethernet (dst = FF:FF:FF:FF:FF:FF)
 *  5. Broadcast Ethernet с ARP
 *  6. Gratuitous ARP Requests (Sender IP == Target IP)
 *  7. Пары ARP targeted request + response
 *  8. Объём данных (байт) между нашим устройством и роутером
 */
class ArpStatistics {
public:
    /**
     * Запускает захват статистики.
     *
     * @param device    сетевой интерфейс
     * @param myMac     наш MAC (для фильтрации трафика с/к нашему устройству)
     * @param routerMac MAC роутера (сначала получите через ArpSender)
     * @param durationSec время захвата в секундах
     */
    void collect(pcpp::PcapLiveDevice* device,
                 const std::string& myMac,
                 const std::string& routerMac,
                 int durationSec);
};
