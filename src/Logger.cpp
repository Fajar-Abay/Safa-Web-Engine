#include "Logger.hpp"
#include <iostream>
#include <vector>

// Instansiasi dari Static objek milik Class global
// Hal ini diperlukan bagi compiler C++ (mendefinisikan tempat nyata memori Mutex static berada)
std::mutex Logger::mtx;
std::vector<std::string> Logger::logs_buffer;

// Implementasi Log Information
void Logger::log(const std::string &msg) {
  // Gembok Pintu Masuk! lock_guard ini memastikan kunci mutex (mtx) ini dibawa,
  // Otomatis terlepas (ter-buka kuncinya) saat bracket } fungsi berakhir
  std::lock_guard<std::mutex> lock(mtx);
  
  std::string line = "[INFO] " + msg;
  // Print Baris INFO
  std::cout << line << std::endl;
  
  // Simpan ke memori buffer utk dibaca UI
  logs_buffer.push_back(line);
  if (logs_buffer.size() > 200) logs_buffer.erase(logs_buffer.begin());
}

// Implementasi Log Error Tersembunyi
void Logger::error(const std::string &msg) {
  // Gembok Pintu!
  std::lock_guard<std::mutex> lock(mtx);
  
  std::string line = "[ERROR] " + msg;
  // std::cerr bukannya std::cout (agar bisa diarahkan lognya ke file 2> linux secara gampang)
  std::cerr << line << std::endl;

  logs_buffer.push_back(line);
  if (logs_buffer.size() > 200) logs_buffer.erase(logs_buffer.begin());
}

std::vector<std::string> Logger::get_recent_logs(size_t limit) {
  std::lock_guard<std::mutex> lock(mtx);
  if (limit == 0 || logs_buffer.size() <= limit) return logs_buffer;
  return std::vector<std::string>(logs_buffer.end() - limit, logs_buffer.end());
}
