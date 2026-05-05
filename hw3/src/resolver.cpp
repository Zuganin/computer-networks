#include "resolver.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <pcap.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

// DNS structs
struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t q_count;
    uint16_t ans_count;
    uint16_t auth_count;
    uint16_t add_count;
};

// Простая функция для конвертации MAC адреса из строки (AA:BB:CC...) в массив 6 байт
void parse_mac(const std::string& mac_str, uint8_t* mac_bytes) {
    int values[6];
    if (sscanf(mac_str.c_str(), "%x:%x:%x:%x:%x:%x", 
               &values[0], &values[1], &values[2], 
               &values[3], &values[4], &values[5]) == 6) {
        for(int i = 0; i < 6; ++i) {
            mac_bytes[i] = (uint8_t)values[i];
        }
    }
}

// Преобразование "hse.ru" в "\x03hse\x02ru\x00"
void format_dns_name(uint8_t* dns_name, const std::string& host) {
    int lock = 0;
    std::string h = host + ".";
    for (size_t i = 0; i < h.length(); i++) {
        if (h[i] == '.') {
            *dns_name++ = i - lock;
            for (; lock < i; lock++) {
                *dns_name++ = h[lock];
            }
            lock++;
        }
    }
    *dns_name++ = '\0';
}

// Вычисление контрольной суммы заголовка (для IP)
uint16_t checksum(uint16_t *buf, int nwords) {
    uint32_t sum;
    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)(~sum);
}

// Декодирование DNS имени (с учетом ссылок 0xC0)
std::string read_dns_name(const uint8_t* reader, const uint8_t* buffer, int* count) {
    std::string name = "";
    int p = 0;
    int jumped = 0;
    int offset;
    *count = 1;

    while (*reader != 0) {
        if (*reader >= 192) { // 0xC0 (сжатие)
            offset = (*reader - 192) * 256 + *(reader + 1);
            reader = buffer + offset - 1;
            jumped = 1;
        } else {
            name += (char)*(reader + 1);
        }
        reader++;
        if (jumped == 0) (*count)++;
    }
    if (jumped == 1) (*count)++;
    
    // Форматируем имя
    std::string formatted_name = "";
    int i = 0;
    int nC = 0;
    while(i < name.length()) {
        nC = (int)name[i];
        for(int j = 0; j < nC; j++) {
            formatted_name += name[i+1+j];
        }
        formatted_name += ".";
        i += nC + 1;
    }
    if (formatted_name.length() > 0) formatted_name.pop_back();
    return formatted_name;
}

void send_dns_query(const std::string& domain, uint16_t query_type, const std::string& target_dns_ip, const std::map<std::string, std::string>& config) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_live(config.at("Название_интерфейса").c_str(), 65536, 1, 1000, errbuf);
    
    if (handle == nullptr) {
        std::cerr << "[-] Ошибка pcap_open_live: " << errbuf << "\n";
        return;
    }

    uint8_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    // 1. Ethernet (14 байт)
    struct ether_header *eth = (struct ether_header *)buffer;
    parse_mac(config.at("MAC_Роутера"), eth->ether_dhost);
    parse_mac(config.at("Мой_MAC"), eth->ether_shost);
    eth->ether_type = htons(ETHERTYPE_IP);

    // 2. IP (20 байт)
    struct ip *iph = (struct ip *)(buffer + sizeof(struct ether_header));
    iph->ip_hl = 5;
    iph->ip_v = 4;
    iph->ip_tos = 0;
    iph->ip_id = htons(54321);
    iph->ip_off = 0;
    iph->ip_ttl = 255;
    iph->ip_p = IPPROTO_UDP;
    iph->ip_src.s_addr = inet_addr(config.at("Мой_IP").c_str());
    iph->ip_dst.s_addr = inet_addr(target_dns_ip.c_str());

    // 3. UDP (8 байт)
    struct udphdr *udph = (struct udphdr *)(buffer + sizeof(struct ether_header) + sizeof(struct ip));
    udph->uh_sport = htons(49152); // Случайный порт источника
    udph->uh_dport = htons(53);    // DNS порт получателя

    // 4. DNS Header (12 байт)
    struct dns_header *dnsh = (struct dns_header *)(buffer + sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct udphdr));
    dnsh->id = htons(0x1234); // Произвольный ID транзакции
    dnsh->flags = htons(0x0100); // Standard query (Recursion Desired)
    dnsh->q_count = htons(1);
    dnsh->ans_count = 0;
    dnsh->auth_count = 0;
    dnsh->add_count = 0;

    // 5. DNS Payload (QNAME + Type + Class)
    uint8_t *qname = (uint8_t *)(buffer + sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct udphdr) + sizeof(struct dns_header));
    format_dns_name(qname, domain);
    
    int qname_len = strlen((const char*)qname) + 1;
    uint16_t *qtype = (uint16_t *)(qname + qname_len);
    uint16_t *qclass = (uint16_t *)(qname + qname_len + 2);
    
    *qtype = htons(query_type); // Type: MX (15) или A (1).
    *qclass = htons(1);         // Class: IN (Internet)

    int dns_payload_len = qname_len + 4;
    int payload_size = sizeof(struct dns_header) + dns_payload_len;
    
    udph->uh_ulen = htons(sizeof(struct udphdr) + payload_size);
    iph->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + payload_size);
    iph->ip_sum = checksum((uint16_t *)iph, sizeof(struct ip) / 2);

    int packet_size = sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct udphdr) + payload_size;

    // Отправка пакета
    std::cout << "[*] Отправка DNS запроса (" << (query_type == 15 ? "MX" : "A") << ") для " << domain 
              << " к " << target_dns_ip << "...\n";
    
    if (pcap_inject(handle, buffer, packet_size) == -1) {
        std::cerr << "[-] Ошибка отправки пакета: " << pcap_geterr(handle) << "\n";
    } else {
        std::cout << "[+] Пакет успешно отправлен в сеть! (Захват ответов ожидайте в Wireshark)\n";
    }

    pcap_close(handle);
}

void resolve_mx(const std::string& domain, const std::map<std::string, std::string>& config) {
    // Тип 15 = MX запись
    send_dns_query(domain, 15, config.at("Провайдерский_DNS"), config);
}

void resolve_roots_and_provider(const std::map<std::string, std::string>& config) {
    std::vector<std::string> domains = {"github.com", "hse.ru", "draw.io"};
    
    std::cout << "\n[== Запросы к корневому серверу (" << config.at("Корневой_DNS") << ") ==]\n";
    for (const auto& d : domains) {
        send_dns_query(d, 1, config.at("Корневой_DNS"), config); // Type 1 = A
    }

    std::cout << "\n[== Запросы к серверу провайдера (" << config.at("Провайдерский_DNS") << ") ==]\n";
    for (const auto& d : domains) {
        send_dns_query(d, 1, config.at("Провайдерский_DNS"), config);
    }
}
