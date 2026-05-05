#include "sniffer.h"
#include <iostream>
#include <pcap.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <csignal>

// Глобальная переменная для остановки сниффера
pcap_t* global_handle = nullptr;

void stop_sniffer(int signum) {
    if (global_handle != nullptr) {
        std::cout << "\n[!] Прерывание перехвата DNS пакетов...\n";
        pcap_breakloop(global_handle);
    }
}

// Структура заголовка DNS (12 байт, RFC 1035)
struct dns_header {
    uint16_t transaction_id;
    uint16_t flags;
    uint16_t q_count; // Количество вопросов
    uint16_t ans_count; // Количество ответов
    uint16_t auth_count; // Authority
    uint16_t add_count; // Additional
};

// Функция-обработчик каждого пойманного пакета
void packet_handler(u_char *user, const struct pcap_pkthdr *header, const u_char *packet) {
    // 1. Уровень L2: Ethernet (14 байт)
    struct ether_header *eth_header;
    eth_header = (struct ether_header *) packet;
    if (ntohs(eth_header->ether_type) != ETHERTYPE_IP) {
        return; // Пропускаем все, что не IPv4
    }

    // 2. Уровень L3: IP заголовок
    struct ip *ip_header;
    ip_header = (struct ip *)(packet + sizeof(struct ether_header)); // +14 байт
    
    if (ip_header->ip_p != IPPROTO_UDP) {
        return; // Пропускаем все, что не UDP
    }

    // 3. Уровень L4: UDP заголовок
    int ip_header_len = ip_header->ip_hl * 4; // Длина вычисляется из 4-битных слов
    struct udphdr *udp_header;
    udp_header = (struct udphdr *)(packet + sizeof(struct ether_header) + ip_header_len);
    
    // 4. Уровень L7: DNS заголовок
    struct dns_header *dns_hdr;
    dns_hdr = (struct dns_header *)(packet + sizeof(struct ether_header) + ip_header_len + sizeof(struct udphdr));

    // Извлекаем адреса для логов
    std::string ip_src = inet_ntoa(ip_header->ip_src);
    std::string ip_dst = inet_ntoa(ip_header->ip_dst);

    // Флаг ответа (QR): 1 бит в самом старшем бите 16-битного flags
    bool is_response = (ntohs(dns_hdr->flags) & 0x8000) != 0;

    std::cout << "\n============================================\n";
    std::cout << "[+] Перехвачен DNS пакет от " << ip_src << ":" << ntohs(udp_header->uh_sport) 
              << " -> " << ip_dst << ":" << ntohs(udp_header->uh_dport) << "\n";
    std::cout << "--- DNS Заголовок ---\n";
    std::cout << "Transaction ID: 0x" << std::hex << ntohs(dns_hdr->transaction_id) << std::dec << "\n";
    std::cout << "Type (QR)     : " << (is_response ? "Response (Ответ)" : "Query (Запрос)") << "\n";
    std::cout << "Questions     : " << ntohs(dns_hdr->q_count) << "\n";
    std::cout << "Answers RRs   : " << ntohs(dns_hdr->ans_count) << "\n";
    std::cout << "Authority RRs : " << ntohs(dns_hdr->auth_count) << "\n";
    std::cout << "Additional RRs: " << ntohs(dns_hdr->add_count) << "\n";
    std::cout << "============================================\n";
}

void start_sniffer(const std::string& interface_name) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;

    // 1. Открываем интерфейс для прослушивания (Promiscuous mode = 1, timeout = 1000ms)
    handle = pcap_open_live(interface_name.c_str(), BUFSIZ, 1, 1000, errbuf);
    if (handle == nullptr) {
        std::cerr << "[-] Ошибка pcap_open_live: " << errbuf << "\n";
        return;
    }

    // Сохраняем handle для возможности прерывания через Ctrl+C
    global_handle = handle;
    signal(SIGINT, stop_sniffer);

    // 2. Настройка BPF-фильтра: ловим только UDP порт 53 (DNS трафик)
    struct bpf_program fp;
    std::string filter_exp = "udp port 53";
    
    // Получаем маску сети интерфейса для оптимизации компиляции фильтра
    bpf_u_int32 mask;
    bpf_u_int32 net;
    if (pcap_lookupnet(interface_name.c_str(), &net, &mask, errbuf) == -1) {
        std::cerr << "[-] Не удалось получить маску сети для " << interface_name << "\n";
        net = 0;
        mask = 0;
    }

    if (pcap_compile(handle, &fp, filter_exp.c_str(), 0, net) == -1) {
        std::cerr << "[-] Ошибка pcap_compile: " << pcap_geterr(handle) << "\n";
        pcap_close(handle);
        return;
    }

    if (pcap_setfilter(handle, &fp) == -1) {
        std::cerr << "[-] Ошибка pcap_setfilter: " << pcap_geterr(handle) << "\n";
        pcap_close(handle);
        return;
    }

    std::cout << "[*] Сниффер запущен на интерфейсе " << interface_name << ". Ожидание DNS пакетов...\n";
    std::cout << "[!] Чтобы остановить и вернуться в меню, нажмите Ctrl+C.\n";

    // 3. Бесконечный цикл перехвата (пока не вызовем pcap_breakloop() на SIGINT)
    pcap_loop(handle, -1, packet_handler, nullptr);

    // Очистка при возврате
    pcap_freecode(&fp);
    pcap_close(handle);
    global_handle = nullptr;
    
    // Восстанавливаем стандартное поведение Ctrl+C
    signal(SIGINT, SIG_DFL);
}
