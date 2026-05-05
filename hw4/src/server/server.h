#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Server {
private:
    int port_;
    boost::asio::io_context io_;
    // tcp::socket socket_;
    tcp::acceptor acceptor_;

public:
    Server(const std::string& config_file);
    ~Server();
    void Start();
};


#endif