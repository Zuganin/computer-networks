#ifndef MESSAGE_H
#define MESSAGE_H
#include <vector>
#include <cstdint>
#include <string>

struct MsgHeader {
    uint32_t messageLength; 
    uint32_t messageID;
};

class Message {
public:
    MsgHeader header;
    std::vector<uint8_t> body;

    static std::vector<uint8_t> SerializeHeader(const MsgHeader& hdr);
    static MsgHeader DeserializeHeader(const std::vector<uint8_t>& buffer);
};

#endif