#include "Logger.hpp"
#include <iostream>

// Instansiasi dari Static objek milik Class global
// Hal ini diperlukan bagi compiler C++ (mendefinisikan tempat nyata memori Mutex static berada)
std::mutex Logger::mtx;

// Implementasi Log Information
void Logger::log(const std::string &msg) {
  // Gembok Pintu Masuk! lock_guard ini memastikan kunci mutex (mtx) ini dibawa,
  // Otomatis terlepas (ter-buka kuncinya) saat bracket } fungsi berakhir
  std::lock_guard<std::mutex> lock(mtx);
  
  // Print Baris INFO
  std::cout << "[INFO] " << msg << std::endl;
}

// Implementasi Log Error Tersembunyi
void Logger::error(const std::string &msg) {
  // Gembok Pintu!
  std::lock_guard<std::mutex> lock(mtx);
  
  // std::cerr bukannya std::cout (agar bisa diarahkan lognya ke file 2> linux secara gampang)
  std::cerr << "[ERROR] " << msg << std::endl;
}
