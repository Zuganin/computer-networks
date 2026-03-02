
#include <string>

/**
 * Config — читает параметры из config.txt (формат KEY=VALUE).
 * Хранит MY_IP, MY_MAC, ROUTER_IP вне исходного кода, как требует задание.
 */
struct Config {
    std::string myIp;
    std::string myMac;
    std::string routerIp;
    std::string iface;   // имя интерфейса, например en0

    /**
     * Загружает конфиг из файла.
     */
    bool load(const std::string& path = "config.txt");

    void print() const;
};
