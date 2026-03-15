cmake_minimum_required(VERSION 3.10)
project(SafaWebServer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add multithreading library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(server 
    src/main.cpp
    src/Server.cpp
    src/Logger.cpp
    src/Utils.cpp
)

target_link_libraries(server PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
