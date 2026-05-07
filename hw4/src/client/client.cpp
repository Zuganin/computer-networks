#include "client.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <boost/system/error_code.hpp>
#include <yaml-cpp/yaml.h>
#include "../common/Message.h"
#include "LocalFileScanner.h"

Client::Client(const std::string& config_file) : client_id_(""), local_dir_(""), server_host_(""), server_port_(5252), io_(), socket_(io_) {
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

void Client::SendMessage(const Message& msg) {
    std::vector<uint8_t> header_bytes = Message::SerializeHeader(msg.header);

    boost::asio::write(socket_, boost::asio::buffer(header_bytes));
    boost::asio::write(socket_, boost::asio::buffer(msg.body));
}

void Client::StartSync() {
    // 1. Отправляем серверу наш ID
    Message connect_msg;
    connect_msg.header.messageID = static_cast<uint32_t>(MessageType::CLIENT_CONNECT);
    
    connect_msg.body.resize(client_id_.size());
    std::memcpy(connect_msg.body.data(), client_id_.c_str(), client_id_.size());
    connect_msg.header.messageLength = connect_msg.body.size();
    
    std::cout << "[Client " << client_id_ << "] Отправляю CLIENT_CONNECT..." << std::endl;
    SendMessage(connect_msg);

    // 2. Сканируем файлы и отправляем FILE_LIST
    auto local_files = LocalFileScanner::ScanDirectory(local_dir_);
    
    // Формат "filename|size|checksum\n"
    std::string payload;
    for (const auto& file : local_files) {
        payload += file.filename + "|" + std::to_string(file.size) + "|" + file.checksum + "\n";
    }

    Message list_msg;
    list_msg.header.messageID = static_cast<uint32_t>(MessageType::FILE_LIST);
    list_msg.body = std::vector<uint8_t>(payload.begin(), payload.end());
    list_msg.header.messageLength = list_msg.body.size();

    std::cout << "[Client " << client_id_ << "] Отправляю FILE_LIST (" << local_files.size() << " файлов)..." << std::endl;
    SendMessage(list_msg);

    // 3. Ждем FILE_SYNC_RESPONSE от сервера
    std::vector<uint8_t> header_buf(8);
    boost::system::error_code ec;
    boost::asio::read(socket_, boost::asio::buffer(header_buf), ec);
    
    if (ec) {
        std::cerr << "[Client] Ошибка при чтении ответа: " << ec.message() << std::endl;
        return;
    }

    MsgHeader resp_hdr = Message::DeserializeHeader(header_buf);
    if (resp_hdr.messageID == static_cast<uint32_t>(MessageType::FILE_SYNC_RESPONSE)) {
        std::vector<uint8_t> body_buf(resp_hdr.messageLength);
        boost::asio::read(socket_, boost::asio::buffer(body_buf));

        std::string payload((char*)body_buf.data(), body_buf.size());
        
        // Разбиваем payload по \n, чтобы получить список нужных файлов
        std::vector<std::string> files_to_send;
        std::string current_name;
        for (char c : payload) {
            if (c == '\n') {
                if (!current_name.empty()) {
                    files_to_send.push_back(current_name);
                    current_name.clear();
                }
            } else {
                current_name += c;
            }
        }
        if (!current_name.empty()) {
            files_to_send.push_back(current_name);
        }

        std::cout << "[Client] Сервер запросил " << files_to_send.size() << " файлов для синхронизации." << std::endl;

        // Отправляем файлы параллельно
        std::vector<std::thread> upload_threads;
        for (const auto& fname : files_to_send) {
            upload_threads.emplace_back([this, fname]() {
                try {
                    boost::asio::io_context thread_io;
                    tcp::socket thread_socket(thread_io);
                    tcp::endpoint endpoint(boost::asio::ip::make_address(server_host_), server_port_);
                    thread_socket.connect(endpoint);

                    // 1. Сначала отправим CLIENT_CONNECT, чтобы сервер знал, чей это файл 
                    Message connect_msg;
                    connect_msg.header.messageID = static_cast<uint32_t>(MessageType::CLIENT_CONNECT);
                    connect_msg.body.resize(client_id_.size());
                    std::memcpy(connect_msg.body.data(), client_id_.c_str(), client_id_.size());
                    connect_msg.header.messageLength = connect_msg.body.size();
                    
                    std::vector<uint8_t> conn_hdr = Message::SerializeHeader(connect_msg.header);
                    boost::asio::write(thread_socket, boost::asio::buffer(conn_hdr));
                    boost::asio::write(thread_socket, boost::asio::buffer(connect_msg.body));

                    // 2. Читаем и отправляем файл кусочками (FILE_CHUNK)
                    std::filesystem::path filepath = std::filesystem::path(local_dir_) / fname;
                    std::ifstream file(filepath, std::ios::binary);
                    if (!file) return;

                    char buffer[262144]; // Отправляем чанками по 256 KB
                    size_t total_sent = 0;
                    
                    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
                        size_t bytes_read = file.gcount();
                        
                        Message chunk_msg;
                        chunk_msg.header.messageID = static_cast<uint32_t>(MessageType::FILE_CHUNK);
                        
                        uint32_t name_len = fname.size();
                        uint32_t name_len_net = htonl(name_len);
                        
                        chunk_msg.body.resize(4 + name_len + bytes_read);
                        std::memcpy(chunk_msg.body.data(), &name_len_net, 4);
                        std::memcpy(chunk_msg.body.data() + 4, fname.c_str(), name_len);
                        std::memcpy(chunk_msg.body.data() + 4 + name_len, buffer, bytes_read);

                        chunk_msg.header.messageLength = chunk_msg.body.size();
                        
                        std::vector<uint8_t> chunk_hdr = Message::SerializeHeader(chunk_msg.header);
                        boost::asio::write(thread_socket, boost::asio::buffer(chunk_hdr));
                        boost::asio::write(thread_socket, boost::asio::buffer(chunk_msg.body));
                        
                        total_sent += bytes_read;
                    }
                    
                    std::cout << "[Client] Файл " << fname << " (" << total_sent << " байт) успешно отправлен [Поток: " << std::this_thread::get_id() << "]" << std::endl;
                    
                    // Закрываем сокет
                    boost::system::error_code ec;
                    thread_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    thread_socket.close(ec);
                    
                } catch (const std::exception& e) {
                    std::cerr << "[Client] Ошибка параллельной отправки " << fname << ": " << e.what() << std::endl;
                }
            });
        }

        // Ждём завершения всех потоков отправок
        for (auto& t : upload_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        std::cout << "[Client] Все запрошенные файлы успешно отправлены." << std::endl;
    }
}
