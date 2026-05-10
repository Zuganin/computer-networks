#ifndef MESSAGE_H
#define MESSAGE_H
#include <vector>
#include <cstdint>
#include <string>

enum class MessageType : uint8_t {
    CLIENT_CONNECT = 1,
    FILE_LIST = 2,
    FILE_SYNC_RESPONSE = 3,
    FILE_CHUNK = 4,
    FILE_TRANSFER_ACK = 5,
    FILE_START_DMA = 6,
    AUTH_OK = 10,
    CLIENT_ALREADY_CONNECTED = 11,
    UPLOAD_AUTH = 12,
};

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