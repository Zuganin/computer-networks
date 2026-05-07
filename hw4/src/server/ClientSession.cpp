#include "ClientSession.h"
#include <string>
#include <memory>
#include <vector>
#include "../common/Message.h"
#include "../client/LocalFileScanner.h" // Подключаем сканер
#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/version.hpp>
#include <filesystem>

// Вспомогательная функция для парсинга payload формата "name|size|hash\n"
std::vector<FileInfo> ParseFileList(const std::string& payload) {
    std::vector<FileInfo> files;
    std::istringstream stream(payload);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.empty()) continue;
        
        std::istringstream linestream(line);
        std::string name, size_str, hash;
        
        if (std::getline(linestream, name, '|') &&
            std::getline(linestream, size_str, '|') &&
            std::getline(linestream, hash)) {
            
            files.push_back({name, std::stoull(size_str), hash});
        }
    }
    return files;
}



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
    body_buffer_.resize(current_header_.messageLength);
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
    if (current_header_.messageID == static_cast<uint32_t>(MessageType::CLIENT_CONNECT)) {

        client_id_ = std::string(body_buffer_.begin(), body_buffer_.end());
        
        std::cout << "[Server] Подключился клиент с ID: " << client_id_ << std::endl;
        
        std::filesystem::path client_path = std::filesystem::path(storage_dir_) / client_id_;
        std::filesystem::create_directories(client_path);
        
        std::cout << "[Server] Директория готова: " << client_path << std::endl;
    }
    else if (current_header_.messageID == static_cast<uint32_t>(MessageType::FILE_LIST)) {
        std::string payload(body_buffer_.begin(), body_buffer_.end());
        auto client_files = ParseFileList(payload);
        
        std::cout << "[Server] Клиент " << client_id_ << " прислал список из " << client_files.size() << " файлов." << std::endl;

        std::filesystem::path client_path = std::filesystem::path(storage_dir_) / client_id_;
        auto server_files = LocalFileScanner::ScanDirectory(client_path.string());

        // Сравниваем и ищем недостающие
        std::string missing_payload;
        int missing_count = 0;

        for (const auto& c_file : client_files) {
            bool found_and_match = false;
            for (const auto& s_file : server_files) {
                if (c_file.filename == s_file.filename && 
                    c_file.size == s_file.size && 
                    c_file.checksum == s_file.checksum) {
                    found_and_match = true;
                    break;
                }
            }
            
            if (!found_and_match) {
                // Сервер узнает нужные файлы
                missing_payload += c_file.filename + "\n";
                missing_count++;

                // Удаляем старый файл, чтобы новые чанки записывались 
                std::filesystem::path file_to_delete = client_path / c_file.filename;
                if (std::filesystem::exists(file_to_delete)) {
                    std::filesystem::remove(file_to_delete);
                }
            }
        }

        // Отправляем ответ клиенту
        Message response_msg;
        response_msg.header.messageID = static_cast<uint32_t>(MessageType::FILE_SYNC_RESPONSE);
        response_msg.body = std::vector<uint8_t>(missing_payload.begin(), missing_payload.end());
        response_msg.header.messageLength = response_msg.body.size();

        std::cout << "[Server] Запрошено " << missing_count << " файлов у клиента " << client_id_ << std::endl;

        std::vector<uint8_t> header_bytes = Message::SerializeHeader(response_msg.header);
        
        // Отправляем заголовок и тело обратно клиенту
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(header_bytes),
            [this, self, response_msg](boost::system::error_code ec, std::size_t) {
                if (!ec) {
                    boost::asio::async_write(socket_, boost::asio::buffer(response_msg.body),
                        [](boost::system::error_code, std::size_t) {});
                }
            });
    }
    else if (current_header_.messageID == static_cast<uint32_t>(MessageType::FILE_CHUNK)) {
        if (body_buffer_.size() < 4) return;

        uint32_t name_len_net;
        std::memcpy(&name_len_net, body_buffer_.data(), 4);
        uint32_t name_len = ntohl(name_len_net);

        if (body_buffer_.size() < 4 + name_len) return;

        std::string fname(reinterpret_cast<char*>(body_buffer_.data() + 4), name_len);
        
        std::filesystem::path client_path = std::filesystem::path(storage_dir_) / client_id_;
        std::filesystem::path filepath = client_path / fname;

        std::ofstream file(filepath, std::ios::binary | std::ios::app);
        if (file) {
            size_t content_size = body_buffer_.size() - 4 - name_len;
            file.write(reinterpret_cast<char*>(body_buffer_.data() + 4 + name_len), content_size);
        }
    }
}