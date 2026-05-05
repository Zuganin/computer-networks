#include "DnsSender.h"
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

uint16_t calculateChecksum(uint16_t* ptr, int nbytes) {
    long sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        sum += *(uint8_t*)ptr;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)~sum;
}

void parseMacAddress(const std::string& macStr, uint8_t* macBytes) {
    unsigned int bytes[6] = {0};
    sscanf(macStr.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", 
           &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]);
    for (int i = 0; i < 6; i++) macBytes[i] = (uint8_t)bytes[i];
}

void encodeDnsName(const std::string& domain, std::vector<uint8_t>& out) {
    size_t pos = 0;
    while (pos < domain.length()) {
        size_t dotPos = domain.find('.', pos);
        if (dotPos == std::string::npos) dotPos = domain.length();
        uint8_t len = dotPos - pos;
        out.push_back(len);
        for (size_t i = pos; i < dotPos; ++i) {
            out.push_back(domain[i]);
        }
        pos = dotPos + 1;
    }
    out.push_back(0); // Null byte
}

std::string readDnsName(const uint8_t* buffer, int& offset, int bufferLimit, int dnsOffset) {
    std::string name;
    int jumped = 0;
    int originalOffset = offset;
    
    while (offset < bufferLimit && buffer[offset] != 0) {
        if ((buffer[offset] & 0xC0) == 0xC0) {
            if (!jumped) originalOffset = offset + 2;
            int pointerOffset = ((buffer[offset] & 0x3F) << 8) | buffer[offset + 1];
            offset = dnsOffset + pointerOffset;
            jumped = 1;
        } else {
            int len = buffer[offset];
            offset++;
            for (int i = 0; i < len && offset < bufferLimit; i++, offset++) {
                name += (char)buffer[offset];
            }
            name += ".";
        }
    }
    if (!jumped) offset++;
    else offset = originalOffset;
    
    if (!name.empty() && name.back() == '.') name.pop_back();
    return name;
}

DnsSender::DnsSender(const Config& cfg) : config(cfg) {}

std::vector<uint8_t> buildRawPacket(const Config& config, const std::string& domain, uint16_t type, const std::string& destIp, uint16_t txId) {
    std::vector<uint8_t> packet;
    
    uint8_t dstMac[6], srcMac[6];
    parseMacAddress(config.routerMac, dstMac); // Шлем на мак шлюза
    parseMacAddress(config.myMac, srcMac);
    
    for (int i = 0; i < 6; i++) packet.push_back(dstMac[i]);
    for (int i = 0; i < 6; i++) packet.push_back(srcMac[i]);
    packet.push_back(0x08); packet.push_back(0x00);

    std::vector<uint8_t> dnsPayload;
    dnsPayload.push_back(txId >> 8); dnsPayload.push_back(txId & 0xFF);
    dnsPayload.push_back(0x01); dnsPayload.push_back(0x00); 
    dnsPayload.push_back(0x00); dnsPayload.push_back(0x01); 
    dnsPayload.push_back(0x00); dnsPayload.push_back(0x00); 
    dnsPayload.push_back(0x00); dnsPayload.push_back(0x00); 
    dnsPayload.push_back(0x00); dnsPayload.push_back(0x00); 

    encodeDnsName(domain, dnsPayload);
    dnsPayload.push_back(type >> 8); dnsPayload.push_back(type & 0xFF);
    dnsPayload.push_back(0x00); dnsPayload.push_back(0x01); 

    std::vector<uint8_t> udpHeader(8, 0);
    uint16_t srcPort = 54321;
    uint16_t dstPort = 53;
    uint16_t udpLen = 8 + dnsPayload.size();
    udpHeader[0] = srcPort >> 8; udpHeader[1] = srcPort & 0xFF;
    udpHeader[2] = dstPort >> 8; udpHeader[3] = dstPort & 0xFF;
    udpHeader[4] = udpLen >> 8;  udpHeader[5] = udpLen & 0xFF;
    
    std::vector<uint8_t> ipHeader(20, 0);
    ipHeader[0] = 0x45; 
    ipHeader[1] = 0x00; 
    uint16_t ipTotalLen = 20 + udpLen;
    ipHeader[2] = ipTotalLen >> 8; ipHeader[3] = ipTotalLen & 0xFF;
    ipHeader[4] = 0x00; ipHeader[5] = 0x00; 
    ipHeader[6] = 0x40; ipHeader[7] = 0x00; 
    ipHeader[8] = 64;   
    ipHeader[9] = 17;   
    
    uint32_t srcIpRaw, dstIpRaw;
    inet_pton(AF_INET, config.myIp.c_str(), &srcIpRaw);
    inet_pton(AF_INET, destIp.c_str(), &dstIpRaw);
    memcpy(&ipHeader[12], &srcIpRaw, 4);
    memcpy(&ipHeader[16], &dstIpRaw, 4);

    uint16_t ipChecksum = calculateChecksum((uint16_t*)ipHeader.data(), 20);
    ipHeader[10] = ipChecksum & 0xFF; ipHeader[11] = ipChecksum >> 8;

    packet.insert(packet.end(), ipHeader.begin(), ipHeader.end());
    packet.insert(packet.end(), udpHeader.begin(), udpHeader.end());
    packet.insert(packet.end(), dnsPayload.begin(), dnsPayload.end());
    
    return packet;
}

void DnsSender::sendDnsQuery(pcap_t* handle, const std::string& domain, uint16_t type, const std::string& dnsServerIp) {
    uint16_t txId = rand() % 65535;
    std::vector<uint8_t> packet = buildRawPacket(config, domain, type, dnsServerIp, txId);
    
    if (pcap_sendpacket(handle, packet.data(), packet.size()) != 0) {
        std::cerr << "[-] Ошибка отправки пакета: " << pcap_geterr(handle) << "\n";
        return;
    }
    
    time_t startTime = time(NULL);
    while (time(NULL) - startTime < 3) {
        struct pcap_pkthdr* header;
        const u_char* pcapData;
        int res = pcap_next_ex(handle, &header, &pcapData);
        if (res > 0) {
            if (header->caplen < 42) continue; 
            int ipOffset = 14;
            if (pcapData[ipOffset + 9] != 17) continue; 
            
            int ihl = (pcapData[ipOffset] & 0x0F) * 4;
            int udpOffset = ipOffset + ihl;
            
            uint16_t srcPort = (pcapData[udpOffset] << 8) | pcapData[udpOffset+1];
            if (srcPort != 53) continue; 
            
            int dnsOffset = udpOffset + 8;
            uint16_t recvTxId = (pcapData[dnsOffset] << 8) | pcapData[dnsOffset+1];
            uint16_t flags = (pcapData[dnsOffset+2] << 8) | pcapData[dnsOffset+3];
            
            if (recvTxId == txId && (flags & 0x8000)) { 
                int ancount = (pcapData[dnsOffset+6] << 8) | pcapData[dnsOffset+7];
                int nscount = (pcapData[dnsOffset+8] << 8) | pcapData[dnsOffset+9];
                int offset = dnsOffset + 12;
                readDnsName(pcapData, offset, header->caplen, dnsOffset); 
                offset += 4;
                
                for (int i = 0; i < ancount; i++) {
                    if (offset >= header->caplen) break;
                    readDnsName(pcapData, offset, header->caplen, dnsOffset);
                    uint16_t rtype = (pcapData[offset] << 8) | pcapData[offset+1];
                    offset += 8;
                    uint16_t rdlength = (pcapData[offset] << 8) | pcapData[offset+1];
                    offset += 2;
                    
                    if (rtype == 15) { 
                        uint16_t pref = (pcapData[offset] << 8) | pcapData[offset+1];
                        int mxOffset = offset + 2;
                        std::string mxServer = readDnsName(pcapData, mxOffset, header->caplen, dnsOffset);
                        std::cout << "[MX] " << mxServer << " (pref " << pref << ")\n";
                        this->findIpForMx(handle, mxServer, domain);
                    } else if (rtype == 1) { 
                        char ipStr[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &pcapData[offset], ipStr, INET_ADDRSTRLEN);
                        std::cout << "[A] " << domain << " -> " << ipStr << "\n";
                    }
                    offset += rdlength;
                }
                if (ancount == 0 && nscount > 0) {
                    std::cout << "[INFO] Ответ содержит NS записи (" << nscount << " шт.):\n";
                    for (int i = 0; i < nscount && offset < header->caplen; i++) {
                        std::string nsDomain = readDnsName(pcapData, offset, header->caplen, dnsOffset);
                        uint16_t rtype = (pcapData[offset] << 8) | pcapData[offset+1];
                        offset += 8;
                        uint16_t rdlength = (pcapData[offset] << 8) | pcapData[offset+1];
                        offset += 2;
                        
                        if (rtype == 2) { // NS запись
                            int nsOffset = offset;
                            std::string nsServer = readDnsName(pcapData, nsOffset, header->caplen, dnsOffset);
                            std::cout << "  -> [NS] Зона " << nsDomain << " управляется сервером: " << nsServer << "\n";
                        } else if (rtype == 6) { // SOA запись
                            std::cout << "  -> [SOA] Информация о зоне " << nsDomain << " (Start of Authority)\n";
                        } else {
                            std::cout << "  -> [Other] Тип записи " << rtype << " для " << nsDomain << "\n";
                        }
                        offset += rdlength;
                    }
                }
                return;
            }
        }
    }
    std::cout << "[Timeout] Ответ не получен.\n";
}

void DnsSender::findIpForMx(pcap_t* handle, const std::string& mxServer, const std::string& originalDomain) {
    uint16_t txId = rand() % 65535;
    std::vector<uint8_t> packet = buildRawPacket(config, mxServer, 1, config.ispDnsIp, txId);
    pcap_sendpacket(handle, packet.data(), packet.size());
    // В реальности тут нужно также ждать pcap_next_ex для A-запроса,
    // но если мы дожидаемся ответа, можно просто добавить аналогичный цикл.
}

void DnsSender::findMxRecord(pcap_t* handle, const std::string& domain) {
    std::cout << "\n>>> Ищем MX запись для " << domain << " (Опрашиваем ISP DNS " << config.ispDnsIp << ") <<<\n";
    sendDnsQuery(handle, domain, 15, config.ispDnsIp); 
}

void DnsSender::runTask3(pcap_t* handle) {
    std::vector<std::string> domains = {"github.com", "hse.ru", "draw.io"};
    std::cout << "\n=== ЗАДАЧА 3: Опрос Корневого сервера (" << config.rootDnsIp << ") ===\n";
    for (const auto& d : domains) {
        std::cout << "\nЗапрашиваем " << d << " у корневого сервера...\n";
        sendDnsQuery(handle, d, 1, config.rootDnsIp);
    }
    std::cout << "\n=== ЗАДАЧА 3: Опрос ISP DNS сервера (" << config.ispDnsIp << ") ===\n";
    for (const auto& d : domains) {
        std::cout << "\nЗапрашиваем " << d << " у ISP DNS сервера...\n";
        sendDnsQuery(handle, d, 1, config.ispDnsIp);
    }
}
