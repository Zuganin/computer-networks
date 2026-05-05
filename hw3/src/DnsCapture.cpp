#include "DnsCapture.h"
#include <iostream>
#include <iomanip>
#include <arpa/inet.h>
#include <netinet/in.h>

// Ручной парсинг DNS по байтам
struct DnsHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

void DnsCapture::start(pcap_t* handle) {
    if (!handle) return;
    
    std::cout << "[DnsCapture] Прослушиваем DNS пакеты. Нажмите Ctrl+C чтобы выйти...\n\n";

    pcap_loop(handle, 0, [](u_char*, const struct pcap_pkthdr* header, const u_char* packet) {
        if (header->caplen < 14) return; // Слишком короткий
        
        // Пропускаем Ethernet
        uint16_t etherType = ntohs(*(uint16_t*)(packet + 12));
        if (etherType != 0x0800) return; // Только IPv4
        
        const u_char* ipHeader = packet + 14;
        if (header->caplen < 14 + 20) return;
        
        uint8_t ipHdrLen = (ipHeader[0] & 0x0F) * 4;
        uint8_t protocol = ipHeader[9];
        
        if (protocol != 17) return; // Только UDP
        
        const u_char* udpHeader = ipHeader + ipHdrLen;
        if (header->caplen < 14 + ipHdrLen + 8) return;
        
        uint16_t srcPort = ntohs(*(uint16_t*)(udpHeader + 0));
        uint16_t dstPort = ntohs(*(uint16_t*)(udpHeader + 2));
        
        if (srcPort != 53 && dstPort != 53) return; // Не DNS
        
        const u_char* dnsBase = udpHeader + 8;
        if (header->caplen < 14 + ipHdrLen + 8 + sizeof(DnsHeader)) return;
        
        const DnsHeader* dnsHeader = (const DnsHeader*)dnsBase;
        
        std::cout << ">>> ЗАХВАЧЕН DNS ПАКЕТ <<<\n";
        bool isResponse = (ntohs(dnsHeader->flags) & 0x8000) != 0;
        
        std::cout << "  Тип:     " << (isResponse ? "Ответ (Response)" : "Запрос (Query)") << "\n";
        std::cout << "  ID:      0x" << std::hex << std::setfill('0') << std::setw(4) << ntohs(dnsHeader->id) << std::dec << "\n";
        std::cout << "  Вопросы: " << ntohs(dnsHeader->qdcount) << "\n";
        std::cout << "  Ответы:  " << ntohs(dnsHeader->ancount) << "\n";
        std::cout << "  Органы (NS): " << ntohs(dnsHeader->nscount) << "\n";
        std::cout << "  Доп. записи: " << ntohs(dnsHeader->arcount) << "\n";
        std::cout << "--------------------------------------------------\n\n";

    }, nullptr);
}
