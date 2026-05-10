#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <boost/asio.hpp>
#include "../common/Message.h"


using boost::asio::ip::tcp;

class Client {
private:
    std::string client_id_;
    std::string local_dir_;
    std::string server_host_;
    int server_port_;
    bool use_dma_;
    boost::asio::io_context io_;
    tcp::socket socket_;

    void SendMessage(tcp::socket& sock, const Message& msg);
    void SendAuth(boost::asio::ip::tcp::socket& sock);
    void ExecuteUploadTask(std::string fname);
    size_t SendFileDMA(tcp::socket& sock, const std::string& fname, const std::string& filepath);
    size_t SendFileStandard(tcp::socket& sock, const std::string& fname, const std::string& filepath);

public:
    Client(const std::string& config_file);
    ~Client();
    void ConnectToServer();
    std::string GetId() const;
    void StartSync();

};

#endif