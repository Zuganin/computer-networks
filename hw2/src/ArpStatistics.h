
#include <pcap/pcap.h>
#include <string>
#include <cstdint>

// ArpStatistics — Задача 3
// Собирает 8 показателей за durationSec секунд.
class ArpStatistics {
public:
    void collect(pcap_t* handle,
                 const std::string& myMac,
                 const std::string& routerMac,
                 int durationSec);
};
