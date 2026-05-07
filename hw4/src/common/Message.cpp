#include "Message.h"
#include <arpa/inet.h>
#include <vector>
#include <cstring>



std::vector<uint8_t>  Message::SerializeHeader(const MsgHeader& hdr) {
    std::vector<uint8_t> buffer(8);

    uint32_t net_lenght = htonl(hdr.messageLength);
    uint32_t net_id = htonl(hdr.messageID);

    std::memcpy(buffer.data(), &net_lenght,4);
    std::memcpy(buffer.data() + 4, &net_id, 4);

    return buffer;
};

MsgHeader Message::DeserializeHeader(const std::vector<uint8_t>& buffer) {
    if(buffer.size() < 8) {
        throw std::runtime_error("Размер буфера слишком мал для MsgHeader");
    }

    MsgHeader hdr;
    uint32_t net_length;
    uint32_t net_id;
    
    std::memcpy(&net_length, buffer.data(), 4);
    std::memcpy(&net_id, buffer.data() + 4, 4);

    hdr.messageLength = ntohl(net_length);
    hdr.messageID = ntohl(net_id);

    return hdr;
}