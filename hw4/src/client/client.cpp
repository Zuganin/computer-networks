#include "client.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <boost/system/error_code.hpp>
#include <yaml-cpp/yaml.h>
#include "../common/Message.h"
#include "LocalFileScanner.h"

#ifdef __APPLE__
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#elif __linux__
#include <sys/sendfile.h>
#include <fcntl.h>
#endif

void Client::SendMessage(tcp::socket& sock, const Message& msg) {
    std::vector<uint8_t> header_bytes = Message::SerializeHeader(msg.header);

    boost::asio::write(sock, boost::asio::buffer(header_bytes));
    boost::asio::write(sock, boost::asio::buffer(msg.body));
}

size_t Client::SendFileDMA(tcp::socket& sock, const std::string& fname, const std::string& filepath) {
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0) return 0;

    size_t fileSize = std::filesystem::file_size(filepath);

    // 1. Отправляем метаданные 1 раз 
    Message start_msg;
    start_msg.header.messageID = static_cast<uint32_t>(MessageType::FILE_START_DMA);
    
    // Формируем тело: [Размер файла (8 байт)] + [Имя файла]
    uint64_t size_net = htonll(fileSize); 
    start_msg.body.resize(8 + fname.size());
    std::memcpy(start_msg.body.data(), &size_net, 8);
    std::memcpy(start_msg.body.data() + 8, fname.c_str(), fname.size());
    
    start_msg.header.messageLength = start_msg.body.size();
    SendMessage(sock, start_msg);

    // 2. Отправляем сырые данные
    off_t offset = 0;
    size_t total_sent = 0;

    while (offset < (off_t)fileSize) {
        size_t bytes_to_send = fileSize - offset; 
        
        int sock_fd = sock.native_handle();
        #if defined(__APPLE__)
        off_t len = bytes_to_send;
        int res = sendfile(fd, sock_fd, offset, &len, nullptr, 0);
        if (res != 0) break;
        size_t sent_now = len;
        #elif defined(__linux__)
        off_t off = offset;
        ssize_t sent_now = sendfile(sock_fd, fd, &off, bytes_to_send);
        if (sent_now <= 0) break; 
        #endif

        offset += sent_now;
        total_sent += sent_now;
    }
    close(fd);
    return total_sent;
}

size_t Client::SendFileStandard(tcp::socket& sock, const std::string& fname, const std::string& filepath) {
    size_t total_sent = 0;
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return 0;

    char buffer[262144];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        size_t bytes_read = file.gcount();
        uint32_t name_len_net = htonl(static_cast<uint32_t>(fname.size()));
        
        Message chunk_msg;
        chunk_msg.header.messageID = static_cast<uint32_t>(MessageType::FILE_CHUNK);
        chunk_msg.body.resize(4 + fname.size() + bytes_read);
        
        std::memcpy(chunk_msg.body.data(), &name_len_net, 4);
        std::memcpy(chunk_msg.body.data() + 4, fname.c_str(), fname.size());
        std::memcpy(chunk_msg.body.data() + 4 + fname.size(), buffer, bytes_read);

        chunk_msg.header.messageLength = chunk_msg.body.size();
        SendMessage(sock, chunk_msg);
        
        total_sent += bytes_read;
    }
    return total_sent;
}

void Client::SendAuth(boost::asio::ip::tcp::socket& sock) {
    Message msg;
    msg.header.messageID = static_cast<uint32_t>(MessageType::CLIENT_CONNECT);
    msg.body.assign(client_id_.begin(), client_id_.end());
    msg.header.messageLength = msg.body.size();
    
    SendMessage(sock, msg);

    std::cout << "[Client " << client_id_ << "] Отправляю CLIENT_CONNECT..." << std::endl;
}

void Client::SendUploadAuth(tcp::socket& sock) {
    Message msg;
    msg.header.messageID = static_cast<uint32_t>(MessageType::UPLOAD_AUTH);
    msg.body.assign(client_id_.begin(), client_id_.end());
    msg.header.messageLength = msg.body.size();
    SendMessage(sock, msg);
}

void Client::ExecuteUploadTask(std::string fname) {
    try {
        boost::asio::io_context t_io;
        tcp::socket t_sock(t_io);
        tcp::endpoint endpoint(boost::asio::ip::make_address(server_host_), server_port_);
        t_sock.connect(endpoint);

        // 1. Авторизация в новом соединении
        SendUploadAuth(t_sock);

        // 2. Отправка файла
        std::string path = (std::filesystem::path(local_dir_) / fname).string();
        auto t1 = std::chrono::high_resolution_clock::now();
        
        size_t sent = use_dma_ ? SendFileDMA(t_sock, fname, path) 
                               : SendFileStandard(t_sock, fname, path);

        auto t2 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
        
        std::cout << "[Client] Файл " << fname << " (" << sent << " байт) отправлен за " 
                  << ms << " ms [Режим: " << (use_dma_ ? "DMA" : "STD") << "]" << std::endl;

        t_sock.shutdown(tcp::socket::shutdown_both);
        t_sock.close();
    } catch (const std::exception& e) {
        std::cerr << "[Thread Error] " << fname << ": " << e.what() << std::endl;
    }
}

Client::Client(const std::string& config_file, int client_index) : client_id_(""), local_dir_(""), server_host_(""), server_port_(5252), use_dma_(false), io_(), socket_(io_) {
    YAML::Node config = YAML::LoadFile(config_file);
    server_host_ = config["server"]["host"].as<std::string>();
    server_port_ = config["server"]["port"].as<int>();
    auto client_cfg = config["clients"][client_index];
    client_id_ = client_cfg["client_id"].as<std::string>();
    local_dir_ = client_cfg["local_dir"].as<std::string>();
    if (client_cfg["use_dma"]) {
        use_dma_ = client_cfg["use_dma"].as<bool>();
    }
    
    std::cout << "[Client " << client_id_ << "] Инициализирован. DMA: " << (use_dma_?"ВКЛ":"ВЫКЛ") << std::endl;
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

void Client::StartSync() {
    // 1. Отправляем серверу наш ID
   
    ConnectToServer();
    SendAuth(socket_);

    std::vector<uint8_t> auth_h_buf(8);
    boost::asio::read(socket_, boost::asio::buffer(auth_h_buf));
    MsgHeader auth_hdr = Message::DeserializeHeader(auth_h_buf);

    if (auth_hdr.messageID == static_cast<uint32_t>(MessageType::CLIENT_ALREADY_CONNECTED)) {
        std::cerr << "Клиент уже подключён...\n";
        socket_.close();
        return; 
    }

    // 2. Сканируем файлы и отправляем FILE_LIST
    auto local_files = LocalFileScanner::ScanDirectory(local_dir_);
    std::string payload;
    for (const auto& file : local_files) {
        payload += file.filename + "|" + std::to_string(file.size) + "|" + file.checksum + "\n";
    }

    Message list_msg;
    list_msg.header.messageID = static_cast<uint32_t>(MessageType::FILE_LIST);
    list_msg.body.assign(payload.begin(), payload.end());
    list_msg.header.messageLength = list_msg.body.size();
    SendMessage(socket_, list_msg);

    // 3. Получение списка нужных файлов
    std::vector<uint8_t> h_buf(8);
    boost::asio::read(socket_, boost::asio::buffer(h_buf));
    MsgHeader resp_hdr = Message::DeserializeHeader(h_buf);
    
    std::vector<uint8_t> b_buf(resp_hdr.messageLength);
    boost::asio::read(socket_, boost::asio::buffer(b_buf));
    std::string to_send_raw((char*)b_buf.data(), b_buf.size());

    // Парсим список файлов (по символу \n)
    std::vector<std::string> files_to_send;
    std::string current_name;
    for (char c : to_send_raw) {
        if (c == '\n') {
            if (!current_name.empty()) 
            { 
                files_to_send.push_back(current_name); current_name.clear(); 
            }
        } else current_name += c;
    }

    std::cout << "[Client] Сервер запросил " << files_to_send.size() << " файлов." << std::endl;

    // 4. Параллельная отправка
    std::vector<std::thread> upload_threads;
    for (const auto& fname : files_to_send) {
        upload_threads.emplace_back(&Client::ExecuteUploadTask, this, fname);
    }

    for (auto& t : upload_threads) {
        if (t.joinable()) t.join();
    }
    
    std::cout << "[Client] Синхронизация успешно завершена." << std::endl;
}
