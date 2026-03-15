#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <map>
#include <string>

/**
 * @struct HttpRequest
 * @brief Cetakan Data Mentah dari Pesan yang dikirim si Google Chrome/Firefox (Klien)
 *
 * Hanya struct wadah "Gudang Penyimpanan" biasa tanpa operasi apapun.
 * Server::parse_request() akan memotong teks-teks ajaib string raksasa menjadi kolom data ini.
 */
struct HttpRequest {
  std::string method;  // Jenis Eksekusi perintahnya (Misal: "GET", "POST", "HEAD")
  std::string path;    // Alamat Map URI yang dicari (Misal: "/index.html" atau "/test_range/")
                       // (Catatan: Path INI SELALU TANPA "?" di ujungnya)
  std::string query;   // Segala tulisan URL SETELAH tanda "?" (Misal "id=10&buku=2")
  std::string version; // Versi protokol penanya klien (Misal: "HTTP/1.1")
  
  // Kamus Induk (MAP) yg mencatat segenap metadata tersembunyi seperti:
  // "host: localhost" 
  // "accept-encoding: gzip"
  std::map<std::string, std::string> headers; 
  
  // Keputusan hemat konektor jaringan. Jika Chrome memberi `Connection: keep-alive`
  // artinya jangan ditutup salurannya kalau dia habis mendownload "index.php" barusan.
  bool keep_alive; 
};

#endif
