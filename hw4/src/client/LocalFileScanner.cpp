#include "LocalFileScanner.h"
#include <iostream>
#include <fstream>
#include <boost/crc.hpp>

std::vector<FileInfo> LocalFileScanner::ScanDirectory(const std::string& directoryPath) {
    std::vector<FileInfo> filesList;
    
    if (!std::filesystem::exists(directoryPath) || !std::filesystem::is_directory(directoryPath)) {
        std::cerr << "Директория не существует: " << directoryPath << std::endl;
        return filesList;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_regular_file()) {
            FileInfo info;
            info.filename = entry.path().filename().string();
            info.size = entry.file_size();
            info.checksum = CalculateChecksum(entry.path().string());
            
            filesList.push_back(info);
        }
    }
    
    return filesList;
}

std::string LocalFileScanner::CalculateChecksum(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) return "";

    boost::crc_32_type result;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        result.process_bytes(buffer, file.gcount());
    }
    if (file.gcount() > 0) {
        result.process_bytes(buffer, file.gcount());
    }

    return std::to_string(result.checksum());
}