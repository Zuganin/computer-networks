#include "ClientSession.h"
#include <string>
#include <memory>
#include <vector>
#include "../common/Message.h"
#include <iostream>
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <filesystem>



ClientSession::ClientSession(tcp::socket socket, const std::string& storage_dir) : socket_(std::move(socket)), storage_dir_(storage_dir) {}

void ClientSession::Start() {
    std::cout << "[ClientSession] Подключился новый клиент." << std::endl;
    ReadHeader();
}

void ClientSession::ReadHeader() {
    header_buffer_.resize(8);

    auto self(shared_from_this()); // Чтобы сессия не уничтожилась, пока идет чтение
    boost::asio::async_read(socket_, boost::asio::buffer(header_buffer_),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                current_header_ = Message::DeserializeHeader(header_buffer_);
                std::cout << "[ClientSession] Получен заголовок. Тип: " << current_header_.messageID 
                          << ", длина: " << current_header_.messageLength << " байт" << std::endl;
                
                ReadBody();
            } else {
                std::cout << "[ClientSession] Клиент отключился." << std::endl;
            }
        });
}

void ClientSession::ReadBody() {
    body_buffer_.resize(8);
    auto self(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(body_buffer_),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                HandleMessage();
                ReadHeader();
            } else {
                std::cout << "[ClientSession] Произошла ошибка чтения сообщения." << std::endl;
            }
        });
}

void ClientSession::HandleMessage() {
    // Здесь мы будем реагировать на разные messageID (1 - Connect, 2 - FileList и тд)
    std::cout << "[ClientSession] Обработка сообщения завершена.\n" << std::endl;
}