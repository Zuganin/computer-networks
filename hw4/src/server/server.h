#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Server {
private:
    int port_;
    std::string storage_dir_;
    boost::asio::io_context io_;
    tcp::acceptor acceptor_;

public:
    Server(const std::string& config_file);
    ~Server();
    void Start();
    void AcceptConnection();
};


#endif