# 📖 Dokumentasi Safa Web Server

> **Versi:** 4.0 | **Bahasa:** C++17 | **Platform:** Linux  
> **Dibuat:** Maret 2026

---

## Daftar Isi

1. [Apa itu Safa Web Server?](#1-apa-itu-safa-web-server)
2. [Arsitektur Sistem](#2-arsitektur-sistem)
3. [Struktur File & Penjelasan Setiap Kode](#3-struktur-file--penjelasan-setiap-kode)
   - [main.cpp](#maincpp)
   - [Server.hpp / Server.cpp](#serverhpp--servercpp)
   - [Connection.hpp](#connectionhpp)
   - [ThreadPool.hpp](#threadpoolhpp)
   - [HttpRequest.hpp](#httprequesthpp)
   - [Logger.hpp / Logger.cpp](#loggerhpp--loggercpp)
   - [Utils.hpp / Utils.cpp](#utilshpp--utilscpp)
4. [Fitur Lengkap & Cara Kerjanya](#4-fitur-lengkap--cara-kerjanya)
5. [Cara Build & Menjalankan](#5-cara-build--menjalankan)
6. [Konfigurasi Virtual Host](#6-konfigurasi-virtual-host)
7. [Cara Maintenance & Pengembangan](#7-cara-maintenance--pengembangan)
8. [Rencana Fitur Berikutnya](#8-rencana-fitur-berikutnya)
9. [Troubleshooting](#9-troubleshooting)

---

## 1. Apa itu Safa Web Server?

Safa Web Server adalah **web server HTTP/HTTPS berperforma tinggi** yang dibangun sepenuhnya dengan C++17. Server ini dirancang seperti Apache atau Nginx namun dengan kode yang lebih ringkas dan mudah dimodifikasi sendiri karena **kamu yang memiliki dan menulis kodenya sendiri**.

**Kemampuan utama:**
- Melayani file statis (HTML, CSS, JS, Gambar, dll)
- Menjalankan skrip PHP lewat mekanisme CGI
- Mendukung koneksi HTTPS via OpenSSL
- Kompresi otomatis GZIP untuk menghemat bandwidth
- Virtual Hosting (1 server, banyak domain/situs)
- Arsitektur event-driven berbasis Linux Epoll (sangat efisien untuk ribuan koneksi serentak)

---

## 2. Arsitektur Sistem

```
┌─────────────────────────────────────────────────────────┐
│                    SAFA WEB SERVER                       │
│                                                          │
│  ┌──────────────┐     ┌──────────────────────────────┐   │
│  │  main.cpp    │────▶│  Server (epoll loop)          │   │
│  │  (entry pt.) │     │  ┌────────────────────────┐  │   │
│  └──────────────┘     │  │  accept() socket baru  │  │   │
│                        │  │         ▼              │  │   │
│  ┌──────────────┐     │  │   Connection wrapper   │  │   │
│  │  Logger      │◀────│  │   (HTTP / SSL/TLS)     │  │   │
│  │  (thread-    │     │  │         ▼              │  │   │
│  │   safe log)  │     │  │   ThreadPool.enqueue() │  │   │
│  └──────────────┘     │  └─────────┬──────────────┘  │   │
│                        │           │                   │   │
│  ┌──────────────┐     │   ┌────────▼────────────┐    │   │
│  │  ThreadPool  │◀────│   │ process_connection() │    │   │
│  │  (worker     │     │   │  ┌────────────────┐ │    │   │
│  │   threads)   │     │   │  │ parse_request  │ │    │   │
│  └──────────────┘     │   │  │ get_vhost_root │ │    │   │
│                        │   │  │ is_safe_path   │ │    │   │
│  ┌──────────────┐     │   │  │    route:      │ │    │   │
│  │  Utils       │◀────│   │  │ ┌────────────┐ │ │    │   │
│  │  (mime type, │     │   │  │ │ .php → CGI │ │ │    │   │
│  │   safe path) │     │   │  │ │ file→static│ │ │    │   │
│  └──────────────┘     │   │  │ │ dir→listing│ │ │    │   │
│                        │   │  │ └────────────┘ │ │    │   │
│                        │   │  └────────────────┘ │    │   │
│                        │   └─────────────────────┘    │   │
│                        └──────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

**Alur kerja singkat:**
1. `main.cpp` membaca argumen CLI → membuat objek `Server`
2. `Server::start()` membuat socket TCP dan mendaftarkannya ke **epoll**
3. Setiap koneksi baru yang masuk dibungkus dalam objek `Connection`
4. `Connection` dikirim ke antrian **ThreadPool** untuk diproses paralel
5. Worker thread menjalankan `process_connection()` → parse HTTP request → routing → kirim respons

---

## 3. Struktur File & Penjelasan Setiap Kode

```
webserver/
├── src/
│   ├── main.cpp          ← Titik masuk program (entry point)
│   ├── Server.hpp        ← Deklarasi class Server
│   ├── Server.cpp        ← Logika utama server (TERBESAR & TERPENTING)
│   ├── Connection.hpp    ← Bungkus koneksi HTTP/HTTPS
│   ├── ThreadPool.hpp    ← Manajemen thread paralel
│   ├── HttpRequest.hpp   ← Struktur data HTTP Request
│   ├── Logger.hpp        ← Deklarasi logger
│   ├── Logger.cpp        ← Thread-safe logging ke terminal
│   ├── Utils.hpp         ← Deklarasi utilitas
│   └── Utils.cpp         ← Fungsi pembantu (mime, path safety)
├── www/                  ← Folder root situs web default
│   ├── index.php
│   ├── style.css
│   └── script.js
├── build/                ← Output kompilasi (jangan edit!)
│   └── server            ← Binary executable
├── cert.pem              ← Sertifikat SSL publik (untuk HTTPS)
├── key.pem               ← Kunci SSL privat (untuk HTTPS)
├── CMakeLists.txt        ← Konfigurasi build
└── README.md             ← Ringkasan singkat
```

---

### `main.cpp`

**Peran:** Titik awal program. Tugasnya hanya membaca argumen dari terminal lalu menyerahkan semua pekerjaan ke objek `Server`.

```
main(argc, argv)
  ↓
Baca argumen: -p (port), -d (doc_root), -t (threads), -c (cert), -k (key)
  ↓
realpath() → verifikasi folder doc_root benar-benar ada di disk
  ↓
Server server(port, doc_root, num_threads, cert, key)
  ↓
server.start() → server berjalan selamanya (blocking loop)
```

**Argumen CLI yang tersedia:**

| Flag | Keterangan | Default |
|------|-----------|---------|
| `-p <port>` | Nomor port TCP | `8000` |
| `-d <path>` | Folder root dokumen web | `./www` |
| `-t <n>` | Jumlah worker thread | Sesuai CPU |
| `-c <file>` | Path file sertifikat SSL (PEM) | _(HTTP saja jika kosong)_ |
| `-k <file>` | Path file kunci SSL privat (PEM) | _(HTTP saja jika kosong)_ |
| `-h` | Tampilkan bantuan | — |

---

### `Server.hpp` / `Server.cpp`

**Peran:** Jantung dari seluruh server. File `.hpp` berisi **deklarasi** (daftar apa saja yang ada), `.cpp` berisi **implementasi** (cara kerjanya).

#### `Server()` — Konstruktor

```cpp
Server(port, doc_root, num_threads, cert_file, key_file)
```

- Menyimpan semua konfigurasi sebagai variabel member
- Jika `cert_file` dan `key_file` tidak kosong → **aktifkan mode HTTPS**
- Inisialisasi OpenSSL: `SSL_CTX_new()` → memuat sertifikat → memuat kunci privat
- Membuat `ThreadPool` dengan `num_threads` worker

---

#### `Server::start()` — Loop Utama Epoll

1. **Buat socket TCP:** `socket(AF_INET, SOCK_STREAM, 0)`
2. **Aktifkan SO_REUSEADDR:** Agar server bisa restart instan tanpa menunggu port bebas
3. **Bind:** Ikat socket ke `0.0.0.0:port`
4. **Listen:** Antrean maksimal 1024 koneksi
5. **Buat epoll fd:** `epoll_create1(0)`
6. **Daftarkan server socket ke epoll:** Event `EPOLLIN | EPOLLET`
7. **Loop:** `epoll_wait()` → tidur hemat CPU sampai ada event
   - Event dari `server_fd` → `accept()` → buat `Connection` → tambah ke epoll
   - Event dari klien → kirim ke `ThreadPool`

> **Kenapa Epoll?** Tanpa epoll, server harus polling setiap socket setiap saat. Dengan epoll, kernel yang memberi tahu saat data benar-benar tersedia. Jauh lebih efisien untuk ribuan koneksi bersamaan.

---

#### `Server::get_vhost_root()` — Virtual Host

```
Request Host: toko.com
doc_root = /home/debay/webserver/www

Cek: /home/debay/webserver/www/toko.com/ (apakah ada?)
  → Ya  → gunakan sebagai root situs
  → Tidak → gunakan doc_root default
```

---

#### `Server::parse_request()` — Parser HTTP

Mengurai teks HTTP mentah menjadi struct `HttpRequest`:
```
"GET /halaman.html?id=5 HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
         ↓
req.method = "GET", req.path = "/halaman.html", req.query = "id=5"
req.headers["host"] = "localhost", req.keep_alive = true
```

---

#### `Server::handle_static()` — Static File + GZIP + Range

**3 jalur pengiriman:**

| Kondisi | Metode | Alasan |
|---|---|---|
| HTTP + file teks compressable | Buffer + gzip_compress | Kirim `Content-Encoding: gzip` |
| HTTP + file besar binary | `sendfile()` zero-copy | Disk→socket langsung di kernel |
| HTTPS + apapun | buffer per-chunk + SSL_write | `sendfile()` tidak kompatibel SSL |

Jika ada header `Range: bytes=X-Y` → kirim `206 Partial Content` untuk streaming/resume download.

---

#### `Server::handle_cgi()` — Eksekusi PHP

```
Browser → Request PHP → Server fork() → php-cgi baca env vars → output → pipe → Server → Browser
```

POST body dibaca dari socket, ditulis ke stdin `php-cgi` lewat `pipe_in` sehingga `$_POST` berfungsi. Output PHP dikompresi GZIP jika browser mendukung.

**Environment vars penting yang dipasang:**

| Var | Nilai | Fungsi di PHP |
|---|---|---|
| `SCRIPT_FILENAME` | path lengkap .php | Path file yang dieksekusi |
| `REQUEST_METHOD` | GET / POST | `$_SERVER['REQUEST_METHOD']` |
| `QUERY_STRING` | `id=5&x=y` | Isi `$_GET` |
| `CONTENT_LENGTH` | panjang body POST | Ukuran `$_POST` |
| `HTTP_*` | semua header | `$_SERVER['HTTP_HOST']` dll |

---

#### `Server::handle_directory_listing()` — UI Daftar Folder

Dipanggil jika direktori tidak punya `index.html`/`index.php`. Membangun HTML tabel dinamis dari hasil `opendir()` + `readdir()` yang menampilkan nama, ukuran, dan tipe setiap file.

---

#### `Server::send_error()` — Respons Error

Mengirim halaman HTML error standar. Digunakan untuk:
- `404 Not Found` — file tidak ketemu
- `403 Forbidden` — akses ditolak
- `416 Range Not Satisfiable` — byte range tidak valid
- `500 Internal Server Error` — gagal fork php-cgi

---

### `Connection.hpp`

**Peran:** Abstraksi socket agar kode di `Server.cpp` tidak perlu peduli apakah koneksi HTTP atau HTTPS.

```cpp
class Connection {
    int fd;         // raw socket file descriptor
    SSL* ssl;       // null jika HTTP biasa
    bool is_https;
};
```

| Method | Fungsi |
|---|---|
| `ensure_handshake()` | Lakukan SSL/TLS handshake pertama kali |
| `read_data(buf, n)` | Baca: `SSL_read` atau `read()` |
| `write_data(buf, n)` | Tulis: `SSL_write` atau `write()` |
| `peek_data(buf, n)` | Intip tanpa consume: `SSL_peek` atau `recv MSG_PEEK` |
| `close_conn()` | Tutup bersih: `SSL_shutdown` + `close(fd)` |

> **Tips:** Kalau ingin tambah WebSocket, cukup tambahkan method baru di `Connection` tanpa ubah `Server.cpp`.

---

### `ThreadPool.hpp`

**Peran:** Pool thread pekerja yang siap mengambil tugas dari antrian.

```
Thread 1 ──┐
Thread 2 ──┤──── [Antrian Tugas (queue)] ◄── enqueue(f)
Thread N ──┘     (mutex-protected FIFO)
```

Setiap thread tidur (`condition_variable::wait`) sampai `enqueue()` memberi notifikasi → ambil task → jalankan → tidur lagi.

**Kenapa tidak buat thread baru per request?** Membuat thread sangat mahal (alokasi stack ~8MB). Thread pool menggunakan thread yang sudah ada, jauh lebih cepat.

---

### `HttpRequest.hpp`

Hanya struct data, tidak ada logika:
```cpp
struct HttpRequest {
    std::string method;   // "GET", "POST"
    std::string path;     // "/halaman.html"
    std::string query;    // "id=5&name=budi"
    std::string version;  // "HTTP/1.1"
    std::map<std::string, std::string> headers;
    bool keep_alive;
};
```

---

### `Logger.hpp` / `Logger.cpp`

```cpp
static std::mutex mtx;  // satu kunci untuk semua thread
Logger::log("pesan");   // [INFO] pesan
Logger::error("error"); // [ERROR] error → ke stderr
```

Mutex mencegah output log bercampur saat banyak thread print bersamaan.

---

### `Utils.hpp` / `Utils.cpp`

#### `get_mime_type(path)`
Map ekstensi → MIME type. Tambah ekstensi baru di map `mime_types` di `Utils.cpp`.

#### `is_safe_path(base_dir, target_path, out)`
Gunakan `realpath()` untuk verifikasi bahwa path yang diresolusi berada di dalam `base_dir`. Mencegah serangan Path Traversal:
```
GET /../../../etc/passwd → realpath() = /etc/passwd → bukan di /www/ → 404
```

---

## 4. Fitur Lengkap & Cara Kerjanya

### 1. 🔄 Linux Epoll

`EPOLLET` (Edge-Triggered): server diberitahu sekali saat data tiba, tidak terus-menerus.  
`EPOLLONESHOT`: satu koneksi hanya diproses satu thread sekaligus, lalu harus di-rearm manual.

---

### 2. 🔒 HTTPS / TLS

```bash
./build/server -p 8443 -c cert.pem -k key.pem
```

Alur: Client Hello → Server Hello + Cert → Key Exchange → Terenkripsi → HTTP request normal.

**Produksi:** Gunakan Let's Encrypt:
```bash
sudo apt install certbot
sudo certbot certonly --standalone -d domain.com
```

---

### 3. 📦 GZIP Compression

`deflateInit2(..., 31, ...)` — angka `31 = 15 + 16` = aktifkan format gzip (bukan deflate biasa).

Aktif jika: browser kirim `Accept-Encoding: gzip` + file bertipe teks.

---

### 4. 🌐 Virtual Hosting

```bash
mkdir -p www/toko.com && echo "<h1>Toko</h1>" > www/toko.com/index.html
echo "127.0.0.1 toko.com" | sudo tee -a /etc/hosts
# Akses: http://toko.com:8000
```

---

### 5. 🐘 PHP CGI

```bash
sudo apt install php-cgi
curl http://localhost:8000/ping.php
```

---

### 6. ⚡ HTTP Range Requests

Header: `Range: bytes=0-1048575` → Respons: `206 Partial Content` + `Content-Range`.  
Digunakan oleh IDM (resume download) dan `<video>` HTML (streaming seek).

---

### 7. 📁 Directory Listing

Auto-aktif jika folder tidak punya `index.html`/`index.php`. Hasil: halaman tabel HTML yang bisa diklik.

---

### 8. 🛡️ Path Traversal Protection

`realpath()` memastikan semua path yang diminta klien benar-benar berada di dalam `doc_root`.

---

### 9. 🔗 Keep-Alive

Koneksi TCP dipertahankan 5 detik setelah request selesai. Hemat overhead TCP handshake untuk request berikutnya dari browser yang sama.

---

### 10. 📝 Thread-Safe Logging

`std::mutex` memastikan satu thread selesai print sebelum thread lain mulai print.

---

## 5. Cara Build & Menjalankan

```bash
# Install dependensi
sudo apt install libssl-dev zlib1g-dev cmake build-essential

# Build
mkdir -p build && cd build && cmake .. && make

# Jalankan HTTP
cd ..
./build/server

# Jalankan HTTPS
./build/server -p 8443 -c cert.pem -k key.pem

# Semua opsi
./build/server -p 80 -d /var/www/html -t 8 -c cert.pem -k key.pem

# Rebuild setelah edit kode
cd build && make
```

---

## 6. Konfigurasi Virtual Host

```bash
# 1. Buat folder dengan nama domain
mkdir -p www/namadomain.local

# 2. Buat file web di dalamnya
echo "<h1>Halo dari namadomain.local!</h1>" > www/namadomain.local/index.html

# 3. Tambahkan domain ke /etc/hosts
echo "127.0.0.1  namadomain.local" | sudo tee -a /etc/hosts

# 4. Jalankan server
./build/server -p 8000

# 5. Akses di browser
# http://namadomain.local:8000
```

---

## 7. Cara Maintenance & Pengembangan

### Menambahkan MIME type baru
Edit `src/Utils.cpp`, tambahkan di map `mime_types`:
```cpp
{".webp", "image/webp"},
{".wasm", "application/wasm"},
```
Lalu `cd build && make`.

---

### Menambahkan HTTP response header
Di `Server.cpp`, fungsi `handle_static()`, cari `std::ostringstream headers;`:
```cpp
headers << "Cache-Control: max-age=3600\r\n"
        << "X-Server: Safa\r\n";
```

---

### Mengubah timeout Keep-Alive
Di `Server.cpp`, fungsi `parse_request()`:
```cpp
tv.tv_sec = 5;  // ← ubah angka ini (detik)
```

---

### Menambahkan endpoint API custom
Di `Server.cpp`, di awal `process_connection()`, sebelum routing utama:
```cpp
if (req.path == "/status") {
    std::string body = R"({"status":"ok","version":"4.0"})";
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                     + std::to_string(body.size()) + "\r\n\r\n" + body;
    conn->write_data(resp.c_str(), resp.size());
    conn->close_conn(); delete conn; return;
}
```

---

### Menambahkan level log baru (WARNING)
Di `src/Logger.hpp`:
```cpp
static void warn(const std::string& msg);
```
Di `src/Logger.cpp`:
```cpp
void Logger::warn(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "[WARN] " << msg << std::endl;
}
```

---

### Menambahkan log ke file
Di `src/Logger.cpp`, tambahkan di atas definisi fungsi:
```cpp
static std::ofstream log_file("/var/log/safa.log", std::ios::app);
```
Dan di setiap fungsi log, tambahkan:
```cpp
if (log_file.is_open()) log_file << "[INFO] " << msg << "\n";
```

---

## 8. Rencana Fitur Berikutnya

### 🗄️ Built-in Memory Cache

**Tujuan:** Simpan file statis yang sering diakses di RAM, skip baca disk.  
**Cara implementasi:** Buat `src/Cache.hpp` dengan `std::map<path, {data, mime, mtime}>` + `std::shared_mutex`. Di `handle_static()`, cek cache dulu, kalau miss baca disk lalu simpan ke cache.

---

### ⚡ io_uring (Async File I/O)

**Tujuan:** Gantikan `read()`/`write()` file biasa dengan antrian async di kernel space menggunakan ring buffer. Drastik lebih efisien untuk I/O file intensif.

**Prasyarat:**
```bash
sudo apt install liburing-dev
```

**Cara implementasi:** Buat `src/IoUring.hpp`, gunakan `io_uring_queue_init`, `io_uring_prep_read`, `io_uring_submit`, `io_uring_wait_cqe`.

---

### 🔀 Reverse Proxy

**Tujuan:** Teruskan request ke backend (Node.js, Python, dll) dan relay responnya ke klien.  
**Cara implementasi:** Buat `src/ProxyHandler.hpp`. Di `process_connection()`, jika path cocok rule (misal `/api/`) → buka socket ke `backend_host:backend_port` → kirim request → relay respons.

---

### ⚖️ Load Balancer

**Tujuan:** Distribusikan beban ke beberapa backend server.  
**Cara implementasi:** Buat `src/LoadBalancer.hpp` dengan `std::vector<Backend>` dan dua algoritma:
- **Round-Robin:** `std::atomic<int> rr_index` yang dienqueue
- **Least-Connections:** tracking `std::atomic<int>` per backend

---

### 🚀 HTTP/3 + QUIC Protocol

**Tujuan:** HTTP di atas UDP + QUIC untuk koneksi lebih cepat (0-RTT) tanpa head-of-line blocking.

**Prasyarat library:** [cloudflare/quiche](https://github.com/cloudflare/quiche) atau [ngtcp2](https://github.com/ngtcp2/ngtcp2)

**Cara implementasi:** Buat `src/QuicServer.hpp` yang listen di UDP port terpisah. QUIC memerlukan implementasi state-machine lengkap: UDP socket, handshake kriptografi, stream multiplexing, loss recovery. Ini adalah fitur paling kompleks dari semua yang ada.

---

## 9. Troubleshooting

| Masalah | Penyebab | Solusi |
|---|---|---|
| `Bind failed: port` | Port sudah dipakai | `lsof -i :8000` → `kill -9 <PID>` |
| PHP tidak jalan | `php-cgi` tidak ada | `sudo apt install php-cgi` |
| `ERR_CERT_INVALID` | Self-signed cert | Klik "Advanced → Proceed" (testing) atau pakai Let's Encrypt (produksi) |
| Build error: `-luring` | liburing tidak ada | `sudo apt install liburing-dev` |
| Build error: OpenSSL | libssl tidak ada | `sudo apt install libssl-dev` |
| Server lambat | Thread kurang / ulimit rendah | `./build/server -t 16` dan `ulimit -n 65535` |
| `Connection reset` | Keep-alive timeout | Normal, browser akan buka koneksi baru |

---

*Dokumentasi Safa Web Server v4.0 — Maret 2026*
