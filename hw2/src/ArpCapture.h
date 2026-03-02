#pragma once
#include <string>

// Forward declaration
namespace pcpp { class PcapLiveDevice; }

/**
 * ArpCapture — Задача 1
 * Захватывает все ARP пакеты в promiscuous режиме и выводит на консоль
 * интерпретацию каждого захваченного кадра.
 *
 * Структура ARP-заголовка (RFC 826):
 *   │ Hardware Type (2B) │ Protocol Type (2B)               │
 *   │ HW Addr Len (1B)   │ Proto Addr Len (1B) │ Op (2B)   │
 *   │ Sender MAC (6B)    │ Sender IP (4B)                   │
 *   │ Target MAC (6B)    │ Target IP (4B)                   │
 */
class ArpCapture {
public:
    /**
     * Запускает захват ARP пакетов.
     * Печатает каждый пакет на консоль. Остановка по нажатию Enter.
     *
     * @param device сетевой интерфейс (уже открытый)
     */
    void start(pcpp::PcapLiveDevice* device);
};
