#include <iostream>
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <openssl/evp.h>
#include <yaml-cpp/yaml.h>

#include "ClientSession.h"
#include "server.h"
#include <iostream>

Server::Server(const std::string& config_file) : io_(), acceptor_(io_) {
    try {
        YAML::Node config = YAML::LoadFile(config_file);
        port_ = config["port"].as<int>();
        storage_dir_ = config["storage_dir"].as<std::string>();
        
        std::filesystem::create_directories(storage_dir_);

        tcp::endpoint endpoint(tcp::v4(), port_);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        std::cout << "[Server] Инициализирован на порту " << port_ << std::endl;
    } catch(const std::exception& e) {
        std::cerr << "[Server] Ошибка конфига: " << e.what() << std::endl;
        throw;
    }
}

Server::~Server() {
    if (acceptor_.is_open()) {
        acceptor_.close();
        std::cout << "[Server] Acceptor закрыт. Сервер завершает работу!" << std::endl;
    }
}

void Server::Start() {
    std::cout << "[Server] Запущен и ожидает подключений..." << std::endl;
    AcceptConnection();
    io_.run();
}

void Server::AcceptConnection() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec ,tcp::socket socket) {
            if (!ec) {
                std::make_shared<ClientSession>(std::move(socket), storage_dir_)->Start();
            }
            AcceptConnection();
        });
}