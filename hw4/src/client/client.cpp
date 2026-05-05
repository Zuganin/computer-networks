#include "client.h"
#include <iostream>
#include <fstream>
#include <boost/system/error_code.hpp>
#include <yaml-cpp/yaml.h>

Client::Client(const std::string& config_file) : io_(), socket_(io_), server_port_(5252), client_id_(""), local_dir_(""), server_host_("") {
    try {
        YAML::Node config = YAML::LoadFile(config_file);
        
        client_id_ = config["client_id"].as<std::string>();
        local_dir_ = config["local_dir"].as<std::string>();
        server_host_ = config["server_host"].as<std::string>();
        server_port_ = config["server_port"].as<int>();
        
        std::cout << "[Client " << client_id_ << "] Инициализирован" << std::endl;
        std::cout << "  Локальная папка: " << local_dir_ << std::endl;
        std::cout << "  Сервер: " << server_host_ << ":" << server_port_ << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[Client] Ошибка при чтении конфига: " << e.what() << std::endl;
        throw;
    }
}
Client::~Client(){
    try {
        if (socket_.is_open()) {
            socket_.close();
            std::cout << "[Client " << client_id_ << "] Соединение закрыто" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Client] Ошибка при закрытии сокета: " << e.what() << std::endl;
    }
}

void Client::ConnectToServer(){
    try{
        socket_.open(tcp::v4());
        tcp::endpoint endpoint(
            boost::asio::ip::make_address(server_host_),
            server_port_
        );
        socket_.connect(endpoint);
    
        std::cout << "[Client " << client_id_ << "] Подключился к серверу " << server_host_ << ":" << server_port_ << std::endl;
    } catch (std::exception& e){
        std::cerr << "[Client " << client_id_ << "] не смог установить соединение: " << e.what() << std::endl;
        throw;
    }
}

std::string Client::GetId() const {
    return client_id_;
}

