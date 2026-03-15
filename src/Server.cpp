#include "Server.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/epoll.h> // Header Khusus Mode Linux Ekstrim Cepat
#include <sys/sendfile.h> // Mode Copy Pipa Zero Copy Tingkat Inti (Kernel)
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <zlib.h> // Library GZIP (Kompresi Teks Respons HTTP)

/**
 * @brief Sistem Pemadat Bandwith (Kompresi Otomatis Data Menjadi Format .Gzip)
 * Menggunakan Pustaka Terkenal ZLib (standar internet)
 */
std::string gzip_compress(const std::string &data) {
  z_stream zs;
  memset(&zs, 0, sizeof(zs));
  
  // Mengatur Mode Deflate menjadi Format Jendela GZIP 31 (15 Bit Standart + 16 Angka Sihir Tanda GZIP)
  if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK)
    return data; // Jika Gagal Init Kompresi Cacat, Kirim Lempar Data Mentah Aja Normal.
    
  zs.next_in = (Bytef *)data.data(); // Tembak Pointer Mulai Kompresi di Data mentah
  zs.avail_in = data.size();         // Targetkan seluruh besaran byte

  char outbuffer[32768];             // Baskom perantara
  std::string outstring;             // Teks Final
  
  // Siklus Remas Compress Selama data asli (avail_out == 0 / Mampat)
  do {
    zs.next_out = reinterpret_cast<Bytef *>(outbuffer);
    zs.avail_out = sizeof(outbuffer);
    int ret = deflate(&zs, Z_FINISH); // Pompa Makanan Asli Menjadi Lemak Gzip
    if (outstring.size() < zs.total_out) {
      outstring.append(outbuffer, zs.total_out - outstring.size()); // Tempelkan
    }
  } while (zs.avail_out == 0);
  deflateEnd(&zs);
  return outstring;
}

// ----------------------------------------------------
// Konstruktor Setup Server
// ----------------------------------------------------
Server::Server(int port, const std::string &doc_root, int num_threads,
               const std::string &cert_file, const std::string &key_file)
    : port(port), doc_root(doc_root), num_threads(num_threads), server_fd(-1),
      epoll_fd(-1), pool(num_threads), cert_file(cert_file), key_file(key_file),
      ssl_ctx(nullptr), is_https(!cert_file.empty() && !key_file.empty()), is_running(false) {

  // Apabila Kunci Keamanan HTTPS ada di parameter CLI, Siapkan Tembok SSLnya.
  if (is_https) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    
    const SSL_METHOD *method = TLS_server_method(); // Gunakan protokol fleksibel modern (mendukung TLS 1.2+ dst)
    ssl_ctx = SSL_CTX_new(method);
    if (!ssl_ctx) {
      Logger::error("Unable to create SSL context");
      exit(EXIT_FAILURE);
    }
    
    // Tanamkan .pem sertifikat publik hasil CA Let's Encrypt / Openssl lokal
    if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file.c_str(),
                                     SSL_FILETYPE_PEM) <= 0) {
      Logger::error("Error loading certificate");
      exit(EXIT_FAILURE);
    }
    
    // Tanamkan Kunci Pribadi .key / .pem 
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file.c_str(),
                                    SSL_FILETYPE_PEM) <= 0) {
      Logger::error("Error loading private key");
      exit(EXIT_FAILURE);
    }
  }
}

Server::~Server() {
  if (server_fd != -1) close(server_fd); // Nutup Soket Port Utma
  if (epoll_fd != -1)  close(epoll_fd);  // Nutup Kamera CCTV pemantau
  if (ssl_ctx)         SSL_CTX_free(ssl_ctx); // Bakar Surat-Surat SSL yang udah dimuat.
}

// ----------------------------------------------------
// METODE MULAINYA ALAM SEMESTA WEB SERVER KITA
// ----------------------------------------------------
void Server::start() {
  // 1. Sewa Jalur Pipa dari Kernel OS (Bernama Socket IPv4 TCP)
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    Logger::error("Socket creation failed");
    return;
  }

  // 2. Minta ke OS Biar Boleh Reboot Jalur Pipa Port Tanpa Penundaan TIMEOUT! (SO_REUSEADDR)
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // 3. Daftarkan Alamat Lokasi Jalur Pipa Port yg diminta
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY; // Listen on ANY interface (0.0.0.0 via Wifi maupun kabel maupun Loopback localhost!)
  address.sin_port = htons(port);       // Host to Network Short: Memutar byte port TCP (Little Endian vs Big Endian)

  // Ikat pipa kita ke Alamat tadi. (Jadilah Server Resmi berdiri di OS ini)
  if (bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0) {
    Logger::error("Bind failed on port " + std::to_string(port));
    return;
  }

  // Buka Loket Gerbang Tamu Konektor
  if (listen(server_fd, 1024) < 0) {
    Logger::error("Listen failed");
    return;
  }

  // == Inisialiasasi Raksasa Kinerja Linux (Epoll) CCTV Ratusan Ribu Event Socket
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    Logger::error("Epoll creation failed");
    return;
  }

  // Daftarkan Socket Utama `server_fd` (Si pencipta anak koneksi) ke Mata Kamera Epoll
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET; // Edge Triggered: Kasihtau Sekali Ajah bila ada getaran input datang.
  ev.data.fd = server_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
    Logger::error("Epoll ctl server_fd failed");
    return;
  }

  // Sapa Admin Server (User)
  std::string scheme = is_https ? "https" : "http";
  Logger::log("Safa Web Server running on port " + std::to_string(port) + " (" +
              scheme + "://localhost:" + std::to_string(port) + ")");
  Logger::log("Document Root: " + doc_root +
              " | Threads: " + std::to_string(num_threads) +
              " | Protocol: " + (is_https ? "HTTPS" : "HTTP"));

  // SIKLUS DETAK JANTUNG TAK BERAKHIR (Infinity Event Loop)
  struct epoll_event events[64]; // Siapkan lembar catatan yg menampung max 64 lapor cctv dalam 1 nafas.
  is_running = true;
  while (is_running) {
    
    // epoll_wait: BIKIN CPU JADI 0% PENGGUNAANYA alias TIDUR NGOROK sampai 
    // Ada laporan yg dikirim System Call Epoll Linux Kernel, ATAU Timeout setiap 1000 md (1 detik).
    int nfds = epoll_wait(epoll_fd, events, 64, 1000);
    
    for (int i = 0; i < nfds; ++i) {
      if (events[i].data.fd == server_fd) {
        // [SCENARIO A] : Yang bergetar adalah CCTV Gerbang Depan (server_fd)
        // Berarti Ada Tamu Baru Nyolok Kabel IP Pengen Main Kesini!
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Ciptakan Anak Tiri File Descriptor Baru (client_fd) yg khusus berbicara antar Dia dan Server saja.
        int client_fd =
            accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd == -1)
          continue; // Oh tamu numpang doang. lanjut biarin dia pegih.. 

        // Tahu Siapa Alamat IP Asli yang datang
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

        // Atur agar Socket anak ini gampang nyerah kalau klien Terdiam membisu selama 5 Detik. Jgn biarin Server kita Kaku.  
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Bungkus Anak Tiri nya jadi Benda OOP yg Tahan HTTPS dan HTTP (Siap Pakai) berserta IP nya
        Connection *conn = new Connection(client_fd, ssl_ctx, ip_str);

        // Tambahkan si Anak Tiri (conn) Ini untuk diawasi Kamera CCTV Baru Oleh Epoll, tapi Cuma dikasih TAU SEKALI AJA KETIKA DIA NGEBACOT data (EPOLLONESHOT)
        struct epoll_event client_ev;
        client_ev.events = EPOLLIN | EPOLLONESHOT;
        client_ev.data.ptr = conn; // Pointer OOP yg dilempar ke Epoll Event Data Cache
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev);
        
      } else {
        // [SCENARIO B] : Yang bergetar adalah Anak Tiri Lama. Dia mau minta Request Kirim Gambar nih/
        Connection *conn = static_cast<Connection *>(events[i].data.ptr);
        
        // Kita JANGAN proses langsung (Biar gak BLOCKING epoll yang lain), tapi TENDANG Anak Ini
        // Untuk diajari Oleh Para Thread Pool (Para Pekerja Asisten CPU) Sambil Meneruskan Looping Epoll!!
        pool.enqueue([this, conn]() { this->process_connection(conn); });
      }
    }
  }

  // Bersihkan resource saat loop selesai (penting agar port bisa di-reuse!)
  Logger::log("Server dihentikan.");
  if (epoll_fd != -1) { close(epoll_fd); epoll_fd = -1; }
  if (server_fd != -1) { close(server_fd); server_fd = -1; }
}

void Server::stop() {
  is_running = false;
}

// ----------------------------------------------------
// UTILS PENCATAT KESALAHAN BROWSER HTTP (Not Found 404/500/403)
// Mendukung Halaman Error Kustom (Misal: baca dari 404.html jika ada)
// ----------------------------------------------------
void Server::send_error(Connection *conn, int status_code,
                        const std::string &status_msg, bool keep_alive) {
  std::string body_str;
  std::string custom_error_file = doc_root + "/" + std::to_string(status_code) + ".html";
  
  // Cek apakah User Membuat Halaman Error Kustom (Misal: 404.html di folder www/)
  int fd = open(custom_error_file.c_str(), O_RDONLY);
  if (fd >= 0) {
    struct stat stat_buf;
    fstat(fd, &stat_buf);
    std::vector<char> buffer(stat_buf.st_size);
    read(fd, buffer.data(), stat_buf.st_size);
    close(fd);
    body_str.assign(buffer.data(), stat_buf.st_size);
  } else {
    // Generate Pesan Error Bawaan Server C++ 
    std::ostringstream body;
    body << "<html><head><title>" << status_code << " " << status_msg
         << "</title></head>"
         << "<body style='font-family: sans-serif; text-align: center; "
            "margin-top: 50px; background-color: #f4f4f9;'>"
         << "<h1>" << status_code << " " << status_msg << "</h1>"
         << "<hr><p style='color: #666;'>Safa Web Server/4.0</p></body></html>";
    body_str = body.str();
  }

  std::ostringstream response;
  response << "HTTP/1.1 " << status_code << " " << status_msg << "\r\n"
           << "Content-Type: text/html\r\n"
           << "Content-Length: " << body_str.length() << "\r\n"
           << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n"
           << "Server: Safa Web Server/4.0\r\n"
           << "X-Content-Type-Options: nosniff\r\n"          // Security Header Penecegah MIME Confusion Hacker
           << "X-Frame-Options: SAMEORIGIN\r\n\r\n"          // Security Header Pencegah Clickjacking
           << body_str;

  std::string final_res = response.str();
  conn->write_data(final_res.c_str(), final_res.length());
}

// ----------------------------------------------------
// Resolusi Otak Virtual Host Domain "toko.com" menjadi Subfolder Target
// ----------------------------------------------------
std::string Server::get_vhost_root(const std::string &host) {
  std::string h = host; 
  size_t colon = h.find(':');
  if (colon != std::string::npos) // Buang Potongan URL titikdua (:8000) dari host header
    h = h.substr(0, colon);

  // Periksa misal foldernya nama /www/toko-laku.com/ 
  std::string vhost_dir = doc_root + "/" + h;
  struct stat st;
  // Jika Folder ITU nyata ada wujudnya (S_ISDIR), kembalikan wujud rutenya sebgai ganti folder aslinya!
  if (stat(vhost_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    return vhost_dir;
  }
  
  // Klo gada folder mirip nama Virtual Host nya, ya balikin target Root awal aj.
  return doc_root;
}

// ----------------------------------------------------
// BEDAH BEDAH DATA MENTAH DARI SELANG PIPA SOCKET KLIEN  
// ----------------------------------------------------
bool Server::parse_request(Connection *conn, HttpRequest &req) {
  char buffer[4096]; // Batas Puncak Panjang Header (4 Kilobytes)
  
  // NGINTIP dulu isinya apa dari Pipa Koneksi. (JANGAN DIMINUM DATANya, alias PEEK!) 
  int bytes = conn->peek_data(buffer, sizeof(buffer) - 1);
  if (bytes <= 0)
    return false; // Error / Kosong

  std::string peeked(buffer, bytes);
  // Cari di mana batasan antara Header Headeran dan Isi Body Request yang sebenarnya ada spasi ganda ("\r\n\r\n")
  size_t header_end = peeked.find("\r\n\r\n");
  std::string raw_req;

  if (header_end == std::string::npos) {
    // Kacau, ukurannya ngelebihi 4KB. Yasudah baca paksa semuanya.
    bytes = conn->read_data(buffer, sizeof(buffer) - 1);
    raw_req.assign(buffer, bytes);
  } else {
    // Nah! Ketebak akhirnya batas kepalanya di titik `header_end + 4`. 
    // Ambil pas pas ukuran Segini, SEHINGGA POST BODY / FILE UPLOAD PHP GA IKUT KEMINUM SAMA KITA!
    size_t exact_size = header_end + 4;
    std::vector<char> exact_buf(exact_size);
    conn->read_data(exact_buf.data(), exact_size);
    raw_req.assign(exact_buf.data(), exact_size);
  }

  // Pecah String menjadi Aliran Stream Layaknya File agar gampang di urai kata demi kata..
  std::istringstream stream(raw_req);
  std::string line;
  if (!std::getline(stream, line)) // Baca Baris 1: Contoh isinya "GET /index.php?tunggu=12 HTTP/1.1"
    return false;

  // Hapus karakter buatan OS Windows ("\r") carriage return
  if (!line.empty() && line.back() == '\r')
    line.pop_back();

  std::istringstream line_stream(line);
  // Masukan kata satu -> GET. kata kedua -> /index.php?tunggu.. ke variabel memory Req struct kita.
  line_stream >> req.method >> req.path >> req.version;

  // Hapus buntut "?" jadi Variabel Query (misal dari /buku?id=5 menjadi query="id=5" ) 
  size_t qpos = req.path.find('?');
  if (qpos != std::string::npos) {
    req.query = req.path.substr(qpos + 1);
    req.path = req.path.substr(0, qpos); // Potong path murni "/buku"
  }

  // Lanjutin Baca Semua Garis Sisa (Header Atribut Chrome) sampe baris itu cuma spasi kosong doang ("\r")
  req.keep_alive = false;
  while (std::getline(stream, line) && line != "\r") {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
      // Pecah tulisan antara titik dua.   Host: asdasd  (jadikan key dan val)
      std::string key = line.substr(0, colon);
      std::string val = line.substr(colon + 1);
      
      val.erase(0, val.find_first_not_of(" \t")); // Ilangin Spasi kosong di Awal val pake Erase.
      
      // Standar W3C Internet, Semua Kepada-Header HTTP itu tidak mempedulikan huruf besar kecil
      // Karena Itu Ubah Semua ke Lowercase Huruf Kecilan aja biar Nyaman!
      for (auto &c : key)
        c = tolower(c);
      
      req.headers[key] = val; // Masukan Info ini ke Database Dictionary (c++ Map)  

      // Jika ada titipan pesan dari Client Keep-Alive, Maka Ubah Niat Kita Jgn Membunuh Koneksi Dini.
      if (key == "connection" && val.find("keep-alive") != std::string::npos)
        req.keep_alive = true;
    }
  }
  return true; // Sukses Mempretelin Semuanya!
}

// ----------------------------------------------------
// GERBANG EKSEKUSI APLIKASI WEB DINAMIS (PHP!) VIA CGI PROCESS FORK
// ----------------------------------------------------
void Server::handle_cgi(Connection *conn, const HttpRequest &req,
                        const std::string &script_path,
                        const std::string &active_doc_root) {
  // Pipa Masuk (Bawa Input Data User POST / JSON / UPLOAD Dari Server kita ke Proses PHP Anak) 
  int pipe_in[2];
  
  // Pipa Keluar (Bawa Balikan Html Dari Proses PHP Anak ke Mulut C++ Server Kita)
  int pipe_out[2];
  
  if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
    send_error(conn, 500, "Internal Server Error", false); // Pipa Rusak.. 
    return;
  }

  std::string post_body; // Gelas Penampungan Sisa Data Klien yg ngirim Metode POST (Contoh form registrasi)
  if (req.method == "POST" && req.headers.count("content-length")) {
    size_t cl = std::stoull(req.headers.at("content-length"));
    if (cl > 10 * 1024 * 1024)
      cl = 10 * 1024 * 1024; // Limit Keamanan mentok POST sampe 10 MB biar ga kepenuhan RAM Server!
      
    std::vector<char> buf(cl);
    conn->read_data(buf.data(), cl);  // Minum Habis Dari Wadah Socket Mentah Klien Asli Tadi. (Tadi kan cmn di intip doang)
    post_body.assign(buf.data(), cl); // Masukan ke Gelas Murni C++ Ini.
  }

  // MEMISAHKAN BADAN KODE MENJADI 2 (Bakal terduplikat dan 1 nya jadi Anak Tiri dari si Safa Server C++ Ini)
  pid_t pid = fork();
  if (pid == -1) {
    send_error(conn, 500, "Internal Server Error", false); 
    return;
  }

  if (pid == 0) {
    // ----------------------------------------------------------
    // [KODE INI DIJALANKAN OLEH SI ANAK FORK -- BUKAN THREAD WORKER]
    // ----------------------------------------------------------

    // Oper Pipa Standart File Input Unix agar menyambung ke Ujung Read / Tulis dari Pipa Kita 
    dup2(pipe_in[0], STDIN_FILENO);   // Mulut PHP Anak (Makan lewat STDIN) nyambung ke Pipa_Masuk punya bapak.
    dup2(pipe_out[1], STDOUT_FILENO); // Bokong / Print-Out PHP Anak (Keluar lewat Echo Stdout) nyambung ke Pipa_Keluar.
    
    // Tutup sisi-sisi pipa yang salah / gaperlu di anak biar gak nyumbat Sistem IO
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);

    // SET ENVIRONMENT VARIABLES DARI APA YG BROWSER TITIP.. BUAT APA?
    // Jawab: PHP pake ini untuk bangun global variable kaya =>  `echo $_SERVER["SERVER_PROTOCOL"];` (Akan Nampilin HTTP/1.1)
    setenv("REDIRECT_STATUS", "200", 1);
    setenv("SCRIPT_FILENAME", script_path.c_str(), 1); // Titik file *.php Utama yg mesti diload 
    setenv("REQUEST_METHOD", req.method.c_str(), 1);   // GET Atau POST
    setenv("QUERY_STRING", req.query.c_str(), 1);      // Biar PHP tau var `$_GET["id"]` 
    setenv("DOCUMENT_ROOT", active_doc_root.c_str(), 1);
    setenv("SERVER_SOFTWARE", "Safa Web Server/4.0", 1);
    setenv("SERVER_PROTOCOL", req.version.c_str(), 1);

    // Kirimin Pula Header Lain-Lainya Tapi depannya dikasih Kata "HTTP_" Buat pembeda (Standard CGI sedunia ini!)
    for (const auto &kv : req.headers) {
      std::string env_name = "HTTP_";
      for (char c : kv.first)
        env_name += (c == '-') ? '_' : toupper(c); // Rubah Strip-Jadi-Underscore (Host-name jadi HTTP_HOST_NAME)
      setenv(env_name.c_str(), kv.second.c_str(), 1);
    }
    
    // Spesial buat POST PHP. Content-Type + Length (Tanpa HTTP_) Biar php sadar var `$_POST` itu ada 
    if (req.headers.count("content-type"))
      setenv("CONTENT_TYPE", req.headers.at("content-type").c_str(), 1);
    if (req.headers.count("content-length"))
      setenv("CONTENT_LENGTH", req.headers.at("content-length").c_str(), 1);

    // EKSEKUSI BUNUH DIRI ANAK LALU GANTI JIWA JADI PROGRAM `PHP-CGI` (Aplikasi external yg terinstall di linux)
    // Otomatis Dia akan hidup menyusui pake pipa diatas serta enviroment yg dikasih, lalu memproses semuanya.
    char *const args[] = {(char *)"php-cgi", NULL};
    execvp("php-cgi", args);
    exit(1); // Kalau sampai tembus ke baris C++ ini (Berarti Di PC Server linuxnya ga di install aplikasi php-cgi / Gagal nyala!)
    
  } else {
    // ----------------------------------------------------------
    // [KODE INI DIJALANKAN SAMA PROGRAM KITA ASLI -- (SI BAPAKNYA / Thread Pool)]
    // ----------------------------------------------------------
    
    close(pipe_in[0]);  // Bapak gak usah dengerin PipaMasuk.
    close(pipe_out[1]); // Bapak gak usah nyemprot PipaKeluaran PHP Anak

    // Wah Anak Lagi kerja kan / Baru Nyala? Kalo ada POST Body, Semprotin skrg makananya ke Pipa 1 Masuk (Biar dimakan PHP CGI di sebrang sana!)
    if (!post_body.empty())
      write(pipe_in[1], post_body.data(), post_body.size());
    close(pipe_in[1]);

    std::string cgi_output;
    char buf[8192];
    int bytes_read;
    
    // NONGKRONG DAN BACA NUNGGUIN SAMPAI SI ANAK BERES KERJA NGODING PHP DAN MUNTAHIN BALIK TULISAN HTML NYA...
    while ((bytes_read = read(pipe_out[0], buf, sizeof(buf))) > 0) {
      cgi_output.append(buf, bytes_read); 
    }
    close(pipe_out[0]);
    
    // Tungguin jg Proses Linux PHP itu mati dan musnah total.. BIar engga nimbulin bug Proses Zombie Hantu!!
    waitpid(pid, NULL, 0);

    // KELAR. Anak Balikan Tulisan Segar Buat Klien Kita! Skrg modifikasi Sedikit Atribut Atribut Hasilnya
    std::string status_line = "HTTP/1.1 200 OK\r\n";
    std::string cgi_headers = "";
    std::string cgi_body = cgi_output;

    // Cari Letak Akhir Kepala Pesan PHP yang misah sama body hasil HTML Echo nya 
    size_t header_end = cgi_output.find("\r\n\r\n");
    if (header_end != std::string::npos) {
      cgi_headers = cgi_output.substr(0, header_end + 2);
      cgi_body = cgi_output.substr(header_end + 4);

      // Kadang PHP Ngasih Header "Status: 404 Data Ga ketemu".
      // Klo nemuin ini, Kita harus ngalah pindahin ini tulisan membuang status 200 OK asli C++ diatas
      // Biar tulisan status ini yang dipakai di Kepalang Balikan Server (Status_Val)
      size_t status_pos = cgi_headers.find("Status: ");
      if (status_pos != std::string::npos) {
        size_t status_end = cgi_headers.find("\r\n", status_pos);
        std::string status_val =
            cgi_headers.substr(status_pos + 8, status_end - (status_pos + 8));
        status_line = "HTTP/1.1 " + status_val + "\r\n";
        cgi_headers.erase(status_pos, status_end - status_pos + 2);
      }
    }

    // GILA AJA. Kita juga Tambahin Skema Pemadat Ukuran! Jikalau browser minta Kompressi, Kompresi Si Hasil PHP Asli tersebut Cepat! 
    bool support_gzip =
        req.headers.count("accept-encoding") &&
        req.headers.at("accept-encoding").find("gzip") != std::string::npos;
    if (support_gzip && cgi_body.length() > 20) {
      cgi_body = gzip_compress(cgi_body);
      cgi_headers += "Content-Encoding: gzip\r\n"; // Infokan Ke Google Chrome, Bahwa Kirimian gw Dikompres nih jagoan
    }

    // Berikan Sapaan Hangat Safa Server Dan Balikkan Jawaban Menembus Langsung Dari Socket ke Depan Muka Pihak Klien
    std::string con_header = req.keep_alive ? "keep-alive" : "close";
    std::ostringstream response;
    response << status_line << "Connection: " << con_header << "\r\n"
             << "Server: Safa Web Server/4.0\r\n"
             << "Content-Length: " << cgi_body.length() << "\r\n"
             << cgi_headers << "\r\n"
             << cgi_body; // Muntahkan. Bam

    std::string final_res = response.str();
    conn->write_data(final_res.c_str(), final_res.length());
  }
}

// ----------------------------------------------------
// UI TAMPILAN DIREKTORI FOLDER (Jika ga ada Index)
// ----------------------------------------------------
void Server::handle_directory_listing(Connection *conn, const HttpRequest &req,
                                      const std::string &real_path,
                                      const std::string &req_path) {
  DIR *dir = opendir(real_path.c_str());
  if (!dir) {
    send_error(conn, 403, "Forbidden", req.keep_alive);
    return;
  }

  // Desain Hantaman UI CSS Langsung ke String Mentah. (Sangat Ringan Tanpa Butuh file terpisah dari C++)
  std::ostringstream body;
  body
      << "<html><head><title>Index of " << req_path << "</title>\n<style>\n"
      << "body { font-family: Inter, sans-serif; background-color: #f6f8fa; "
         "margin: 0; padding: 40px; color: #333; }\n"
      << ".container { max-width: 900px; margin: auto; background: white; "
         "padding: 20px; border-radius: 8px; box-shadow: 0 4px 6px "
         "rgba(0,0,0,0.1); }\n"
      << "h1 { margin-top: 0; font-size: 24px; color: #24292e; border-bottom: "
         "2px solid #eaecef; padding-bottom: 10px; }\n"
      << "table { width: 100%; border-collapse: collapse; margin-top: 15px; }\n"
      << "th, td { padding: 12px 15px; text-align: left; border-bottom: 1px "
         "solid #eaecef; }\n"
      << "th { background-color: #f6f8fa; color: #57606a; font-weight: 600; }\n"
      << "tr:hover { background-color: #f3f4f6; }\n"
      << "a { text-decoration: none; color: #0969da; font-weight: 500; "
         "display: block; }\n"
      << "a:hover { text-decoration: underline; }\n"
      << "p.footer { text-align: center; color: #8c959f; font-size: 13px; "
         "margin-top: 20px; }\n"
      << "</style></head><body>\n<div class='container'>\n<h1>Index of "
      << req_path << "</h1>\n"
      << "<table><tr><th>Name</th><th>Size</th><th>Type</th></tr>\n";

  // Tombol Kembali ke Folder Sang Bapak Jikalau Direktori saat ini Bukanlah Dasar (Gelar root "/")
  if (req_path != "/") {
    std::string parent = req_path;
    if (parent.back() == '/')
      parent.pop_back();
    size_t last_slash = parent.find_last_of('/');
    if (last_slash != std::string::npos)
      parent = parent.substr(0, last_slash + 1);
    else
      parent = "/";
    body << "<tr><td><a href='" << parent
         << "'>📁 ../ (Parent Directory)</a></td><td>-</td><td>Dir</td></tr>\n";
  }

  struct dirent *ent;
  std::vector<struct dirent *> entries;
  
  // Baca Satuan Isi Item Folder pakai OS Readdir..
  while ((ent = readdir(dir)) != NULL) {
    std::string name = ent->d_name;
    if (name == "." || name == "..")
      continue; // Buang Simbol Bawaan Folder OS Sampah Biar ga Tampil di UI.
    entries.push_back(ent); // Simpan nama buat Nanti Dirapikan!
  }
  
  // Mengurutkan String Berdasarkan Ascending (A sampai Z) Abjad Indah!
  std::sort(entries.begin(), entries.end(),
            [](struct dirent *a, struct dirent *b) {
              return std::string(a->d_name) < std::string(b->d_name);
            });

  // Render Baris per Baris Tampilan Tiap File Folder Pada Tabel
  for (auto e : entries) {
    std::string name = e->d_name;
    std::string full_path = real_path;
    if (full_path.back() != '/')
      full_path += '/';
    full_path += name;

    struct stat sbuf;
    if (stat(full_path.c_str(), &sbuf) != 0) // Jika Gagal Minta metadata ukuran OS, Lompati Item ini!
      continue;

    // Kalkulator Kilobytes dan Megabytes Cantik 
    std::string type = "File", size = std::to_string(sbuf.st_size) + " B";
    if (sbuf.st_size > 1024 * 1024)
      size = std::to_string(sbuf.st_size / (1024 * 1024)) + " MB";
    else if (sbuf.st_size > 1024)
      size = std::to_string(sbuf.st_size / 1024) + " KB";

    // Siapkan Link yang bisa di Klik Google Chrome (Misal "halaman.html" akan dicopot lalu disambung Ke Path URL)
    std::string url_path = req_path;
    if (url_path.back() != '/')
      url_path += '/';
    url_path += name;
    
    std::string display_name = "📄 " + name; // Default File

    if (S_ISDIR(sbuf.st_mode)) { // Tanda Spesial Kalau Isinya Adalah FOLDER / DIREKTORI
      display_name = "📁 " + name + "/";
      type = "Dir";
      size = "-";
      url_path += "/";
    }

    body << "<tr><td><a href='" << url_path << "'>" << display_name
         << "</a></td><td>" << size << "</td><td>" << type << "</td></tr>\n";
  }
  closedir(dir);
  body << "</table>\n<p class='footer'>Safa Web "
          "Server/4.0</p>\n</div></body></html>";

  // Fitur Kompresi GZIP Juga Berlaku! Jadi Kinerja Responsnya Sangat Instan 1 Milidetik!
  std::string body_str = body.str();
  bool support_gzip =
      req.headers.count("accept-encoding") &&
      req.headers.at("accept-encoding").find("gzip") != std::string::npos;
  std::string enc = "";
  if (support_gzip) {
    body_str = gzip_compress(body_str);
    enc = "Content-Encoding: gzip\r\n";
  }

  std::ostringstream response;
  response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
           << body_str.length() << "\r\n"
           << enc << "Connection: " << (req.keep_alive ? "keep-alive" : "close")
           << "\r\nServer: Safa Web Server/4.0\r\n\r\n"
           << body_str;

  // Hantam Responsnya 
  std::string final_res = response.str();
  conn->write_data(final_res.c_str(), final_res.length());
}

// ----------------------------------------------------
// PENGEKSEKUSI DATA STATIS CEPAT (FOTO/JS/CSS DLL) 
// ----------------------------------------------------
void Server::handle_static(Connection *conn, const HttpRequest &req,
                           const std::string &file_path) {
                           
  // Buka gembok file OS Langsung (Kalo bisa Cepat knp pake std::ifstream yang lambat?)
  int fd = open(file_path.c_str(), O_RDONLY);
  if (fd < 0) {
    send_error(conn, 404, "Not Found", req.keep_alive);
    return;
  }

  // Timbang Berapa Besar Bobot Datanya
  struct stat stat_buf;
  fstat(fd, &stat_buf);

  // Jika yang diminta Adalah File tapi Ternyata nyasar Isinya Adalah Folder.. Haram!
  if (S_ISDIR(stat_buf.st_mode)) {
    close(fd);
    send_error(conn, 403, "Forbidden", req.keep_alive);
    return;
  }

  // Cek Apakah CSS Gambar / Dll 
  std::string mime = Utils::get_mime_type(file_path);
  std::string con_header = req.keep_alive ? "keep-alive" : "close";

  // Inisialiasi Persiapan Pemotongan Muatan Besar File (Untuk Streaming / Resumable Download)
  off_t offset = 0;          // Start ngangkat dari detik/byte ke berapa
  size_t count = stat_buf.st_size; // Ambil Data sebanyak byte ini saja (Total Aslinya)
  int status_code = 200;     // OK
  std::string status_msg = "OK";
  std::string range_header_response = "";

  // Nah Apakah Client Punya Permintaan Nyeleneh? Seperti minta File Part ke-X (Range Bytes)?
  if (req.headers.count("range")) {
    std::string range_header = req.headers.at("range");
    if (range_header.find("bytes=") == 0) {
      std::string range = range_header.substr(6);
      size_t dash = range.find('-'); // Cari Pemisah Start-Stop (misal bytes=5000- )
      if (dash != std::string::npos) {
        std::string start_str = range.substr(0, dash);
        std::string end_str = range.substr(dash + 1);
        try {
          if (!start_str.empty())
            offset = std::stoll(start_str); // Oh Start Dari 5 Ribu!
          if (!end_str.empty())
            count = std::stoll(end_str) - offset + 1; // Dan batas max Sampai byte sekian!
          else
            count = stat_buf.st_size - offset; // Atau Mentokin angkat Sampai Seterusnya!

          // Validasi Umpan yg Ngaco!
          if (offset >= stat_buf.st_size || count <= 0) {
            close(fd);
            send_error(conn, 416, "Range Not Satisfiable", req.keep_alive);
            return;
          }
          
          // Mantap Klien Berhasil Men-download ulang setengah Jalan / Streaming Video Sukses Merespons Partial Content
          status_code = 206;
          status_msg = "Partial Content";
          range_header_response = "Content-Range: bytes " +
                                  std::to_string(offset) + "-" +
                                  std::to_string(offset + count - 1) + "/" +
                                  std::to_string(stat_buf.st_size) + "\r\n";
        } catch (...) {
        }
      }
    }
  }

  // ALGORITMA PENUNJUK TIPE PERFOMA APA SEBAIKNYA PENGIRIMAN FILE ASSET INI BERJALAN?
  bool support_gzip =
      req.headers.count("accept-encoding") &&
      req.headers.at("accept-encoding").find("gzip") != std::string::npos;
      
  // Pastikan file itu bisa dikompres! Jangan kompres format JPEG/PDF/ZIP yang aslinya emang udah ketat biner nya. Malah jadi gede datanya! 
  bool compressable = (mime.find("text/") != std::string::npos ||
                       mime.find("application/") != std::string::npos) &&
                      (mime.find("pdf") == std::string::npos &&
                       mime.find("zip") == std::string::npos);
                       
  bool use_gzip = support_gzip && compressable && (status_code == 200);

  // Jika Kita Ada Mode Pengaman HTTPS | Atau Harus dikompres dulu | Atau Memang Isinya sangat Ringan (Kurang dari 1 MB).. Kita Gunakan Mode READ C++ STANDARD (Pakai Buffer Memory RAM).
  if (is_https || use_gzip || count < 1024 * 1024) {
      
    // WADUH! Kalau file Guede Banget (>1 MB) TAPI dia di HTTPS. Kita gabisa pake Memory Mentok 1 Giga Karena Ram Server Bisa JEBOL!!! Kasihan Server.
    // Berarti Mode Ini Pilihan Satu satunya : Chunked Loop Terbatas! (Kirim Pelan-Pelan via SSL_Write) 
    if (is_https && !use_gzip &&
        count >= 1024 * 1024) { 
      std::ostringstream headers;
      // Berikan Header Awal Dulu Semuanya Beserta Security Headers Anti XSS
      headers << "HTTP/1.1 " << status_code << " " << status_msg
              << "\r\nContent-Type: " << mime << "\r\n"
              << "Content-Length: " << count << "\r\nAccept-Ranges: bytes\r\n"
              << "X-Content-Type-Options: nosniff\r\n"
              << "X-Frame-Options: SAMEORIGIN\r\n"
              << range_header_response << "Connection: " << con_header
              << "\r\nServer: Safa Web Server/4.0\r\n\r\n";
      std::string h = headers.str();
      conn->write_data(h.c_str(), h.length());

      // Pindahkan Posisi Titik Awal Data Yg Diminta (Sesuai Range Offset)
      lseek(fd, offset, SEEK_SET);
      char chunk[16384]; // Batas gayung memori 16 KB Aja! Gausah Gede Gede
      size_t remaining = count;
      
      // Ambil pakai Gayung C++ dan ceburkan ke Saluran Pengeluaran Socket TLS secara Berulang sampe Air (isi file disk murni) Habis
      while (remaining > 0) {
        size_t to_read = std::min((size_t)sizeof(chunk), remaining);
        int r = read(fd, chunk, to_read);
        if (r <= 0)
          break; // Ups Datanya Habis Terhenti atau Koneksi jebol
        conn->write_data(chunk, r);
        remaining -= r;
      }
      close(fd);
      return; 
    }

    // Ah Filenya Kecil atau Kita Harus Pake GZIP. (Muat Kok ke RAM). Sedot Keseluruhan!
    std::vector<char> buffer(count);     // Bikin Baskom Khusus Segede count (file yg diminta).
    lseek(fd, offset, SEEK_SET);         // geser target ke awal range
    read(fd, buffer.data(), count);      // Minum air Disk ke Buffer Vector RAM
    close(fd);

    std::string body(buffer.data(), count);
    if (use_gzip) {
      body = gzip_compress(body); // Olah Kompres! Maknyus kecil Skrg body nya
      count = body.length();      // Ganti ukuran Asli jadi Total Yg udah dikompres 
    }

    // SIAP KIRIM! Kasih Header Pembuka
    std::ostringstream headers;
    headers << "HTTP/1.1 " << status_code << " " << status_msg
            << "\r\nContent-Type: " << mime << "\r\n"
            << "Content-Length: " << count << "\r\nAccept-Ranges: bytes\r\n"
            << "X-Content-Type-Options: nosniff\r\n"
            << "X-Frame-Options: SAMEORIGIN\r\n"
            << range_header_response;
    if (use_gzip)
      headers << "Content-Encoding: gzip\r\n";
    if (is_https) 
      headers << "Strict-Transport-Security: max-age=31536000\r\n"; // HSTS Header
    headers << "Connection: " << con_header
            << "\r\nServer: Safa Web Server/4.0\r\n\r\n";

    std::string h = headers.str();
    conn->write_data(h.c_str(), h.length()); // Send Header
    conn->write_data(body.c_str(), count);   // Send Isi Asset Yg Ada Di RAM Memory Ini
    
  } else {
      
    // MODE KETIGA: PALING MAKSIMAL (ZERO-COPY KERNEL LINUX)!
    // Ini aktif klo kita gapunya beban SSL dan beban GZIP dan filenya Lumayan Gede!!.
    
    std::ostringstream headers;
    headers << "HTTP/1.1 " << status_code << " " << status_msg
            << "\r\nContent-Type: " << mime << "\r\n"
            << "Content-Length: " << count << "\r\nAccept-Ranges: bytes\r\n"
            << "X-Content-Type-Options: nosniff\r\n"
            << "X-Frame-Options: SAMEORIGIN\r\n"
            << range_header_response << "Connection: " << con_header
            << "\r\nServer: Safa Web Server/4.0\r\n\r\n";
    std::string h = headers.str();
    conn->write_data(h.c_str(), h.length());

    // Fitur Magis Linux SENDFILE()
    // RAM kita sama sekali gapernah menyentuh / menyimpan 1 byte gambar asset pun!
    // Langsung pindahin pointer Hardisk (fd) ke pointer Tembakan socket (conn->fd) Di level inti Linux. (Memori Aman 100MB pun instan sedetik).
    sendfile(conn->fd, fd, &offset, count); 
    close(fd);
  }
}

// ----------------------------------------------------
// PENANGGUNG JAWAB UTAMA LOGIKA PENILAIAN DARI KLIEN MASUK (Jantung Worker Thread)
// ----------------------------------------------------
void Server::process_connection(Connection *conn) {
  HttpRequest req;
  
  // Baca Data Menjadi Variabel Struct.
  // Jika gagal baca (koneksi jebol / iseng dari peretas port), Tutup dan Copot Pendaftaranya dari CCTV Epoll
  if (!parse_request(conn, req)) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->fd, nullptr);
    conn->close_conn();
    delete conn;
    return;
  }

  Logger::log("[" + conn->client_ip + "] " + req.method + " " + req.path); // Console Log "[127.0.0.1] GET /halokes"

  // Fitur Validasi Sub-Domain Virtual Hosting Otomatis!
  std::string host =
      req.headers.count("host") ? req.headers["host"] : "default";
  std::string active_doc_root = get_vhost_root(host);

  std::string target = req.path;
  if (target.empty() || target == "/")
    target = "/index.php"; // Default Arah Folder Root ditargetkan Cek index.php 

  std::string raw_path = active_doc_root + target;
  std::string real_path;

  // Lapis Pengaman Hacking OS (Local File Inclusion / Path Traversal Hack)
  if (!Utils::is_safe_path(active_doc_root, raw_path, real_path)) {
    // Nah! Kalau Dia Minta `/index.php` dan Gagal ketemu/gadapat, Kita banting setir jatuhnya ke `/index.html` sebagai alternatif 
    if (target == "/index.php") {
      raw_path = active_doc_root + "/index.html";
      if (!Utils::is_safe_path(active_doc_root, raw_path, real_path))
        send_error(conn, 404, "Not Found", req.keep_alive); // Mentok kaga dua dwanya yasudah ERROR 404 KASIAN.
      else
        goto handle_found_fallback; // KETEMU! Loncatkan Programnya pakai GOTO Biar Diekesekusi lgsg dan ngga duplikat!
    } else
      send_error(conn, 404, "Not Found", req.keep_alive);
  } else {
  handle_found_fallback:
    struct stat sbuf;
    if (stat(real_path.c_str(), &sbuf) == 0) {
      if (S_ISDIR(sbuf.st_mode)) {
          
        // OMG. Yang di Minta URLnya itu FOLDER murni, Bukan File langsung.
        // Server pintar Kita akan Mencarikan `index.php` di dalam mapel folder itu otomatis!
        std::string index_php = real_path;
        if (index_php.back() != '/')
          index_php += "/";
        index_php += "index.php";
        
        std::string index_html = real_path;
        if (index_html.back() != '/')
          index_html += "/";
        index_html += "index.html";

        struct stat idx_buf;
        if (stat(index_php.c_str(), &idx_buf) == 0)
          handle_cgi(conn, req, index_php, active_doc_root); // Cihuy ketemu PHP
        else if (stat(index_html.c_str(), &idx_buf) == 0)
          handle_static(conn, req, index_html);              // Atau Paling jelek ketemu HTML nya.
        else
          handle_directory_listing(conn, req, real_path, target); // Jikalau Kosong Melompong, Buatkan LISTING VISUAL Cantik Dir. Folder nya.
          
      } else {
          
        // OH TENANG NYA.. Yang diminta Murni FILE Biasa.
        // Apakah ini File Script Dinamis Khusus Pengeksekusi Back-end Kita? 
        if (real_path.length() >= 4 &&
            real_path.substr(real_path.length() - 4) == ".php")
          handle_cgi(conn, req, real_path, active_doc_root); // Kasihkan Anak PHP. Biar dia bereskan ke Browser Klien
        else
          handle_static(conn, req, real_path); // Oh Tidak, file Foto Biasa, Teruskan Pindah Zero Copy/GZip ke Klien Browser.
      }
    } else
      send_error(conn, 404, "Not Found", req.keep_alive); // File Fiktif, Tolak Kasih Error Standard.
  }

  // Apakah kita Sudah kelar ngasih respons? IYA KITA SUDAH SELESAI NGASIH HASILNYA SEKARANG.
  // Tapi Cek dulu, Apakah Browser Mau Nahan Saluran nya (Keep-Alive)?
  if (req.keep_alive) {
    // Buka Mata Kamera Epoll Sekali Lagi EPOLLONESHOT Buat Koneksi Anak ini, Dan Cek Kalau Kalau dia BacoT ngomong request gambar/html lain dimasa depan
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = conn;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
  } else {
    // Hapus total keberadaan Tanda tangan CCTV Epoll Nya. Dan Usir Anak Socket C++ OOP Ini dr memori. Tutup Semuanya. Mati!.
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->fd, nullptr);
    conn->close_conn();
    delete conn;
  }
}
