
#include <pcap/pcap.h>
#include <string>

// ArpSender — Задача 2
// Отправляет ARP Request к роутеру и получает его MAC из ARP Reply.
class ArpSender {
public:
    // Возвращает MAC роутера (пустая строка если не получен)
    std::string findRouterMac(pcap_t* handle,
                               const std::string& myIp,
                               const std::string& myMac,
                               const std::string& routerIp,
                               int timeoutMs = 3000);
};
