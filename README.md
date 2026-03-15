# Safa Web Server (C++)

> **Versi 4.0** | HTTP + HTTPS | Epoll | GZIP | Virtual Host | PHP CGI | Zero-Copy

---

## Cara Cepat Menjalankan

```bash
# HTTP biasa
./build/server

# HTTPS (sudah ada cert.pem & key.pem di sini)
./build/server -p 8443 -c cert.pem -k key.pem

# Semua opsi
./build/server -p <port> -d <folder_web> -t <jumlah_thread> -c cert.pem -k key.pem
```

---

## Fitur Utama (10 Fitur)

| # | Fitur | Keterangan Singkat |
|---|-------|--------------------|
| 1 | **Epoll Event Loop** | Menangani ribuan koneksi efisien di Linux |
| 2 | **Static File + GZIP** | Sajikan HTML/CSS/JS dengan kompresi otomatis |
| 3 | **HTTPS / TLS** | Enkripsi via OpenSSL, aktifkan dengan `-c -k` |
| 4 | **Virtual Hosting** | Multi-domain: buat subfolder `www/namadomain.com/` |
| 5 | **PHP CGI** | Jalankan skrip `.php` via `php-cgi` |
| 6 | **HTTP Range Requests** | Resume download & streaming video |
| 7 | **Directory Listing** | Tampilan daftar folder otomatis jika tidak ada index |
| 8 | **Path Traversal Guard** | Blok akses file di luar folder web |
| 9 | **Keep-Alive** | Satu koneksi TCP untuk banyak request |
| 10 | **Thread-Safe Logger** | Log aman dari banyak thread sekaligus |

---

## Cara Build

```bash
mkdir -p build && cd build
cmake ..
make
```

**Dependensi yang harus ada:**
```bash
sudo apt install libssl-dev zlib1g-dev
```

---

## Dokumentasi Lengkap

Baca [`DOCS.md`](DOCS.md) untuk:
- Penjelasan setiap baris kode penting
- Cara konfigurasi virtual host
- Cara menambahkan fitur baru
- Troubleshooting lengkap
- Rencana fitur: io_uring, HTTP/3, cache, reverse proxy, load balancer

---

## Struktur Proyek

```
src/
├── main.cpp          ← Entry point & CLI args
├── Server.hpp/cpp    ← Inti server (epoll, routing, gzip)
├── Connection.hpp    ← Abstraksi HTTP/HTTPS socket
├── ThreadPool.hpp    ← Worker threads
├── HttpRequest.hpp   ← Struktur data request
├── Logger.hpp/cpp    ← Thread-safe logging
└── Utils.hpp/cpp     ← MIME type & path safety
www/                  ← Letakkan file web di sini
cert.pem / key.pem    ← Sertifikat SSL (testing)
```
