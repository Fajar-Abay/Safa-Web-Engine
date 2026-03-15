#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

/**
 * @class Connection
 * @brief Membungkus socket (file descriptor) agar mendukung HTTP maupun HTTPS transparan.
 * 
 * Class ini berfungsi sebagai "Abstraction Layer" (lapisan abstraksi). 
 * Di Server.cpp, kita tidak perlu repot memilah apakah ini koneksi biasa (TCP)
 * atau terenkripsi (TLS). Cukup memanggil method read_data/write_data pada objek ini.
 */
class Connection {
public:
  int fd;               // File descriptor dari socket klien hasil accept()
  SSL *ssl;             // Pointer ke objek SSL openSSL (null jika HTTP biasa)
  bool is_https;        // Flag penanda apakah ini koneksi aman (HTTPS)
  bool ssl_accepted;    // Penanda apakah jabat-tangan (handshake) SSL sudah selesai
  std::string client_ip;// Alamat IP Asli pengunjung

  /**
   * @brief Konstruktor koneksi baru
   * @param fd file descriptor milik socket klien
   * @param ctx konteks SSL dari server (berisi sertifikat/kunci). Null jika HTTP.
   * @param ip string IP address dari klien
   */
  Connection(int fd, SSL_CTX *ctx, const std::string& ip = "Unknown")
      : fd(fd), ssl(nullptr), is_https(ctx != nullptr), ssl_accepted(false), client_ip(ip) {
    if (ctx) {
      ssl = SSL_new(ctx);   // Membangun struktur SSL baru dari context
      SSL_set_fd(ssl, fd);  // Mengaitkan struktur SSL ke file descriptor socket
    }
  }

  /**
   * @brief Memastikan proses jabat-tangan koneksi HTTPS sudah beres
   * @return true jika siap dipakai, false jika koneksi gagal
   */
  bool ensure_handshake() {
    if (!is_https)
      return true; // Jika HTTP biasa, tidak butuh handshake
    if (ssl_accepted)
      return true; // Handshake sudah pernah diselesaikan

    // Lakukan SSL_accept untuk menegosiasikan kunci pengaman dengan browser
    int ret = SSL_accept(ssl);
    if (ret <= 0) {
      return false; // Gagal negosiasi
    }
    ssl_accepted = true;
    return true;
  }

  /**
   * @brief Terjemahan fungsi read() yang aman. (Akan memanggil SSL_read jika https)
   */
  ssize_t read_data(void *buf, size_t count) {
    if (!ensure_handshake())
      return -1;
    if (is_https)
      return SSL_read(ssl, buf, count); // dekripsi data dari lalu-lintas jaringan
    return ::read(fd, buf, count);      // baca telanjang dari port socket (http plaintext)
  }

  /**
   * @brief Terjemahan fungsi write() secara universal. 
   */
  ssize_t write_data(const void *buf, size_t count) {
    if (!ensure_handshake())
      return -1;
    if (is_https)
      return SSL_write(ssl, buf, count); // enkripsi payload ke browser klien
    return ::write(fd, buf, count);
  }

  /**
   * @brief Mengintip data masuk tanpa mengonsumsinya dari buffer stream
   * Gunakan ini untuk memastikan apakah line tersebut adalah \r\n\r\n tanpa menelan isinya.
   */
  ssize_t peek_data(void *buf, size_t count) {
    if (!ensure_handshake())
      return -1;
    if (is_https)
      return SSL_peek(ssl, buf, count);
    return ::recv(fd, buf, count, MSG_PEEK);
  }

  /**
   * @brief Tutup memori SSL dan tutup File Descriptor socket dengan tata krama baik.
   */
  void close_conn() {
    if (ssl) {
      SSL_shutdown(ssl); // Ucapkan selamat tinggal pada TLS
      SSL_free(ssl);     // Lepas memori OpenSSL
      ssl = nullptr;
    }
    if (fd >= 0) {
      ::close(fd);       // Tutup jalur pipa aslinya ke klien
      fd = -1;
    }
  }
};

#endif
