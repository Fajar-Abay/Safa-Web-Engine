#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <mutex>
#include <string>
#include <vector>

/**
 * @class Logger
 * @brief Pencatat terminal yang kebal ditabrak dari berbagai thread (Thread-safe Printout).
 *
 * Fungsi static memudahkan di-import dari file.cpp acak (kapan saja) tanpa harus "new Logger" dulu.
 */
class Logger {
public:
  /**
   * @brief Mencetak Teks Pesan Informatif yang berwarna ke konsol
   * @param msg Teks yang ingin di display
   */
  static void log(const std::string &msg);
  
  /**
   * @brief Mencetak Teks Gagal / Kerusakan (STDERR Linux) 
   * @param msg Teks peringatan yang ingin dikirim
   */
  static void error(const std::string &msg);

  /**
   * @brief Membaca cuplikan riwayat Log yang baru dicetak
   * @param limit Batas maksimum baris (misal 50 terakhir)
   * @return std::vector dari string text line logs
   */
  static std::vector<std::string> get_recent_logs(size_t limit = 100);

private:
  static std::vector<std::string> logs_buffer;

  // Mutex raksasa global (Semaphore pintu masuk printout terminal standard OS)
  // Tidak peduli jika 10 koneksi terhubung dan nyala paralel mendadak bersamaan dan mencoba di-log,
  // mutex menjamin cuma antri SATU persatu agar baris teks output printah layar tidak berantakan hancur tumpah-tindih.
  static std::mutex mtx;
};

#endif
