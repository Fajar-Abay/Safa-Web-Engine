#ifndef SERVER_HPP
#define SERVER_HPP

#include "Connection.hpp"
#include "HttpRequest.hpp"
#include "ThreadPool.hpp"
#include <openssl/ssl.h>
#include <string>

/**
 * @class Server
 * @brief Otak Utama Penggerak Sistem Aplikasi Safa Web Server C++.
 *
 * Di sinilah jantung semua request dari client bermuara untuk
 * diterjemahkan kedalam epoll (Loop Utama Event Asinkron)
 * sampai dialirkan menjadi sebuah respons final (kiriman teks/gambar file).
 */
class Server {
public:
  /**
   * @brief Mempersiapkan Segala Kebutuhan Web Server
   *        Termasuk membuat Epoll fd (File Descriptor) Kernel OS, 
   *        OpenSSL Sertifikat Koneksi Keamanan, serta memanggil Kelas ThreadPool
   */
  Server(int port, const std::string &doc_root, int num_threads,
         const std::string &cert_file, const std::string &key_file);
  ~Server(); // Membersihkan sampah fd yang terpakai dan Memory SSL saat Server Matot

  /**
   * @brief Memulai "Infinity Loop" Utama Tanpa Henti Melayani Klien TCP.
   */
  void start();

private:
  /**
   * @brief Loop Pelayan (Worker Task) yg Mengolah Rute Kehidupan Dari HTTP Klien
   */
  void process_connection(Connection *conn);
  
  /**
   * @brief Mencomot Data Mentahan Koneksi Mentah Jadi sebuah Variabel Struct Lengkap
   */
  bool parse_request(Connection *conn, HttpRequest &req);
  
  /**
   * @brief Penembak Proses Ke PHP (Common Gateway Interface via Fork & Exec)
   */
  void handle_cgi(Connection *conn, const HttpRequest &req,
                  const std::string &script_path,
                  const std::string &active_doc_root);
                  
  /**
   * @brief Memanfaatkan OS untuk menembakkan Image/HTML via teknik Zero-Copy 100% Cepat Memori.
   */
  void handle_static(Connection *conn, const HttpRequest &req,
                     const std::string &file_path);
                     
  /**
   * @brief Pembuat Laman Tabel UI cantk Saat Directory Diminta tanpa index file
   */
  void handle_directory_listing(Connection *conn, const HttpRequest &req,
                                const std::string &real_path,
                                const std::string &req_path);
                                
  /**
   * @brief Pengirim Teks HTML Standard Status "Error 404/500/403" Ke Klien
   */
  void send_error(Connection *conn, int status_code,
                  const std::string &status_msg, bool keep_alive);

  /**
   * @brief Memecahkan Misteri Kemana Arah Multi-Site Domain (Virtual Hosting Map) Dituju
   */
  std::string get_vhost_root(const std::string &host);

  // Kumpulan Parameter Fundamental System Server:
  int port;             // Port Akses Pengguna
  std::string doc_root; // Kandang folder data statik
  int num_threads;      // Batas Max CPU Pekerja Thread
  int server_fd;        // Penampung File Descriptor Tuan Rumah (Main Socket)
  int epoll_fd;         // Penampung Cermin Interupsi File Descriptor Epoll Kernel
  ThreadPool pool;      // Tempat Pekerja Duduk Tunggu Orderan Masuk

  // Variabel Pilihan jika SSL HTTPS Dipilih Aktif
  std::string cert_file;
  std::string key_file;
  SSL_CTX *ssl_ctx;     // "Konteks Mesin OpenSSL" yang memuat Segala Setting Konfigurasi SSL 
  bool is_https;        // Sakelar Logis Switch On/Off dari Fitur Gembok HTTPS
};

#endif
