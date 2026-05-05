#include <iostream>
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <openssl/evp.h>
#include <yaml-cpp/yaml.h>

#include "server.h"
#include <iostream>

Server::Server(const std::string& config_file) : io_(), acceptor_(io_) {
    try{
        YAML::Node config = YAML::LoadFile(config_file);
        port_ = config["port"].as<int>();
        std::cout << "[Server] Успешно инициализирован!" << std::endl;

    } catch(std::exception& e) {
         std::cerr << "[Server] Ошибка при чтении конфига: " << e.what() << std::endl;
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
    try {
        // Создаем endpoint (адрес, на котором слушать)
        tcp::endpoint endpoint(tcp::v4(), port_);
        
        // Открываем acceptor для слушания
        acceptor_.open(tcp::v4());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        
        std::cout << "[Server] Слушаем на 127.0.0.1:" << port_ << std::endl;
        
        // Бесконечный цикл принятия клиентов
        while (true) {
            tcp::socket socket(io_);
            acceptor_.accept(socket);  // БЛОКИРУЕМСЯ здесь, ждем клиента
            
            std::cout << "[Server] Новый клиент подключился: " 
                      << socket.remote_endpoint().address() << std::endl;
            
            // TODO: Отправить приветствие клиенту или запустить обработчик
            socket.close();
        }
        
    } catch (std::exception& e) {
        std::cerr << "[Server] Ошибка запуска сервера: " << e.what() << std::endl;
        throw;
    }
}
