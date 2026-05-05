cmake_minimum_required(VERSION 3.15)
project(hw3_dns CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── PcapPlusPlus (установлен через brew) ───────────────────────────────────
find_package(PcapPlusPlus QUIET)

if(PcapPlusPlus_FOUND)
    message(STATUS "Found PcapPlusPlus via CMake config")
    # Хедеры: PcapPlusPlus_INCLUDE_DIR задаётся cmake конфигом
    include_directories(${PcapPlusPlus_INCLUDE_DIR})
    # Таргеты: PcapPlusPlus::Pcap++  PcapPlusPlus::Packet++  PcapPlusPlus::Common++
    set(PCPP_LIBS PcapPlusPlus::Pcap++ PcapPlusPlus::Packet++ PcapPlusPlus::Common++)
else()
    # Fallback: ищем вручную (brew на Apple Silicon / Intel)
    if(APPLE)
        if(EXISTS "/opt/homebrew/include/pcapplusplus")
            set(PCPP_INCLUDE "/opt/homebrew/include/pcapplusplus")
            set(PCPP_LIB_DIR "/opt/homebrew/lib")
        elseif(EXISTS "/usr/local/include/pcapplusplus")
            set(PCPP_INCLUDE "/usr/local/include/pcapplusplus")
            set(PCPP_LIB_DIR "/usr/local/lib")
        else()
            message(FATAL_ERROR "PcapPlusPlus not found. Run: brew install pcapplusplus")
        endif()
    endif()

    find_library(PCPP_COMMON  Common++  PATHS ${PCPP_LIB_DIR} REQUIRED)
    find_library(PCPP_PACKET  Packet++  PATHS ${PCPP_LIB_DIR} REQUIRED)
    find_library(PCPP_PCAP    Pcap++    PATHS ${PCPP_LIB_DIR} REQUIRED)
    find_library(PCAP_LIB     pcap      REQUIRED)

    set(PCPP_LIBS ${PCPP_PCAP} ${PCPP_PACKET} ${PCPP_COMMON} ${PCAP_LIB})
    include_directories(${PCPP_INCLUDE})
    message(STATUS "PcapPlusPlus found manually at ${PCPP_INCLUDE}")
endif()

# ─── Исходники ───────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/Config.cpp
    src/DnsCapture.cpp
    src/DnsSender.cpp
    src/DnsCapture.cpp src/DnsSender.cpp
)

add_executable(dns_tool ${SOURCES})
target_link_libraries(dns_tool PRIVATE ${PCPP_LIBS})

# На macOS нужны дополнительные frameworks
if(APPLE)
    target_link_libraries(dns_tool PRIVATE "-framework CoreFoundation" "-framework SystemConfiguration")
endif()

# Отключаем предупреждения из заголовков сторонних библиотек
target_compile_options(dns_tool PRIVATE -Wall -Wextra)
