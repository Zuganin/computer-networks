#ifndef SCANNER_H
#define SCANNER_H
#include <string>
#include <vector>
#include <filesystem>


struct FileInfo {
    std::string filename;
    size_t size;
    std::string checksum;
};

class LocalFileScanner {
private:
    static std::string CalculateChecksum(const std::string& filepath);
public:
    static std::vector<FileInfo> ScanDirectory(const std::string& directoryPath);  
};

#endif