#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Client {
private:
    std::string client_id_;
    std::string local_dir_;
    std::string server_host_;
    int server_port_;
    boost::asio::io_context io_;
    tcp::socket socket_;

public:
    Client(const std::string& config_file);
    ~Client();
    void ConnectToServer();
    std::string GetId() const;
    
};

#endif