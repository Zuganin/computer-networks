#ifndef CLIENTSESSION_H
#define CLIENTSESSION_H

#include <string>
#include <memory>
#include <vector>
#include "../common/Message.h"
#include <iostream>
#include <boost/asio.hpp>
#include <filesystem>

using boost::asio::ip::tcp;

class ClientSession : public std::enable_shared_from_this<ClientSession> {
private:
    tcp::socket socket_;
    std::string storage_dir_;
    std::string client_id_;
    
    std::vector<uint8_t> header_buffer_;
    std::vector<uint8_t> body_buffer_;
    MsgHeader current_header_;

    void ReadHeader();
    void ReadBody();
    void HandleMessage();

public:
    ClientSession(tcp::socket socket, const std::string& storage_dir);
    void Start();

};

#endif