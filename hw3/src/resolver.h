#ifndef RESOLVER_H
#define RESOLVER_H

#include <string>
#include <map>

// Функция для отправки DNS-запроса (MX или A)
void send_dns_query(const std::string& domain, uint16_t query_type, const std::string& target_dns_ip, const std::map<std::string, std::string>& config);

// Запуск пункта 2 (MX-записи)
void resolve_mx(const std::string& domain, const std::map<std::string, std::string>& config);

// Запуск пункта 3 (Корневые и провайдерские серверы)
void resolve_roots_and_provider(const std::map<std::string, std::string>& config);

#endif // RESOLVER_H
