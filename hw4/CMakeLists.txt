cmake_minimum_required(VERSION 3.15)
project(MyDrive CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# --- Поиск зависимостей ---

# Boost (Asio)
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# yaml-cpp
find_package(yaml-cpp REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)

# --- Группировка исходных файлов ---

# Файлы, которые нужны И клиенту, И серверу
set(COMMON_SOURCES
    src/common/Message.cpp
    src/client/LocalFileScanner.cpp
)

# Исходники только для клиента
set(CLIENT_ONLY_SOURCES
    src/client/main.cpp
    src/client/client.cpp
)

# Исходники только для сервера
set(SERVER_ONLY_SOURCES
    src/server/main.cpp
    src/server/server.cpp
    src/server/ClientSession.cpp
)

# --- Сборка Клиента ---

add_executable(mydrive_client 
    ${CLIENT_ONLY_SOURCES}
    ${COMMON_SOURCES}
)

target_link_libraries(mydrive_client PRIVATE 
    yaml-cpp::yaml-cpp
    OpenSSL::Crypto
)

# --- Сборка Сервера ---

add_executable(mydrive_server 
    ${SERVER_ONLY_SOURCES}
    ${COMMON_SOURCES}
)

target_link_libraries(mydrive_server PRIVATE 
    yaml-cpp::yaml-cpp 
    OpenSSL::Crypto
)


message(STATUS "✓ CMake configuration successful!")
message(STATUS "  Targets created: mydrive_client, mydrive_server")