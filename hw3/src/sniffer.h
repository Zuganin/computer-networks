#ifndef SNIFFER_H
#define SNIFFER_H

#include <string>

// Функция запускает бесконечный перехват DNS пакетов на указанном интерфейсе
void start_sniffer(const std::string& interface_name);

#endif // SNIFFER_H
