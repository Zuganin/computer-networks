#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <string>
#include <memory>
#include <vector>
#include "../common/Message.h"
#include <iostream>
#include <boost/asio.hpp>
#include <unordered_set>
#include <fstream>
#include <filesystem>

using boost::asio::ip::tcp;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
private:
    tcp::socket socket_;
    std::string storage_dir_;
    std::string client_id_;
    std::unordered_set<std::string>& active_clients_;
    
    std::vector<uint8_t> header_buffer_;
    std::vector<uint8_t> body_buffer_;
    MsgHeader current_header_;

    bool is_dma_mode_ = false;
    uint64_t dma_bytes_expected_ = 0;
    std::ofstream dma_file_;
    std::vector<char> dma_buffer_;

    void SendHeaderOnly(MessageType type);

    void ReadHeader();
    void ReadBody();
    void HandleMessage();

    void ReadRawDMA();

public:
    ClientSession(tcp::socket socket, const std::string& storage_dir, std::unordered_set<std::string>& active_clients);
    void Start();

};

#endif