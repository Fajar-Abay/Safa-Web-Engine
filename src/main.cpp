#include "Logger.hpp"
#include "Server.hpp"

#include <climits>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

/**
 * @file main.cpp
 * @brief Titik Pintu Masuk (Entry Point) Utama untuk Safa Web Server
 * Pengurai perintah command-line dari Terminal
 */
int main(int argc, char *argv[]) {
  // 1. Variabel Default Konfigurasi Awal
  int port = 8000;                     // Port default http://localhost:8000
  std::string raw_doc_root = "./www";  // Map folder target root (document root)
  std::string cert_file = "";          // Path ke berkas keamanan TLS Publik (.pem)
  std::string key_file = "";           // Path ke berkas keamanan TLS Privat (.pem / .key)

  // 2. Tentukan otomatis berkat perangkat keras prosesor komputer host
  int num_threads = std::thread::hardware_concurrency(); // Berapa inti CPU yang kita siapkan
  if (num_threads == 0) // Jika OS mendeteksi gagal (tidak disupport), paksa panggil 4 pekerja
    num_threads = 4;

  // 3. Parser argumen dari CLI pengguna. (Contoh: ./server -p 8080 -d /var/html -t 8)
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-p" && i + 1 < argc)
      port = std::stoi(argv[++i]);       // String dikonversi masuk sebagai Nomor TCP
    else if (arg == "-d" && i + 1 < argc)
      raw_doc_root = argv[++i];          // Override direktori
    else if (arg == "-t" && i + 1 < argc)
      num_threads = std::stoi(argv[++i]);// Override banyak slot thread
    else if (arg == "-c" && i + 1 < argc)
      cert_file = argv[++i];             // Aktifkan Sertifikat Public SSL
    else if (arg == "-k" && i + 1 < argc)
      key_file = argv[++i];              // Kunci SSL
    else if (arg == "-h") {              // Fitur Bantuan (--help)
      std::cout << "Usage: " << argv[0]
                << " [-p port] [-d doc_root] [-t threads] [-c cert.pem] [-k key.pem]\n"
                << "Note: Supply both -c and -k to enable HTTPS.\n";
      return 0; // Kembalikan tanpa menyalakan server
    }
  }

  // 4. Resolve Direktori absolut dari String Relatif menggunakan algoritma realpath() OS.
  // Pastikan folder Document root (-d yang dicantumkan) itu TIDAK SALAH ATAU KOSONG
  char base_real[PATH_MAX];
  if (!realpath(raw_doc_root.c_str(), base_real)) {
    Logger::error("Document root does not exist: " + raw_doc_root);
    return 1; // Keluar (gagal berjalan) jika foldernya fiktif (tidak dibikin oleh pengguna)
  }
  std::string doc_root(base_real); // Merubah dari char C absolut yang sah ke String C++

  // 5. Inisialisasikan Mesin Inti Safa Server 
  // Pindahkan peranan (Pass Data Konfigurator) ke Dalam Kelas OOP `Server.hpp`
  Server server(port, doc_root, num_threads, cert_file, key_file);
  
  // 6. Jalankan siklus tak-terbatas (Blocking Event Loop) Epoll
  server.start();

  // (Abaikan ini, sebab server.start() dibuat infinite loop jadi line program main tidak akan jatuh kesini)
  return 0;
}