#include "Utils.hpp"
#include <cctype>
#include <climits>
#include <cstdlib>
#include <map>

std::string Utils::get_mime_type(const std::string &path) {
  // Kamus Pintar Penyimpan Kamus Format Web Standar Statis
  // static map agar kamus ini cukup dibangun (memory allocation) sekali 
  // sepanjang komputasi server hidup, agar super cepat.
  static std::map<std::string, std::string> mime_types = {
      {".html", "text/html"},        {".htm", "text/html"},
      {".css", "text/css"},          {".js", "application/javascript"},
      {".json", "application/json"}, {".png", "image/png"},
      {".jpg", "image/jpeg"},        {".jpeg", "image/jpeg"},
      {".gif", "image/gif"},         {".svg", "image/svg+xml"},
      {".ico", "image/x-icon"},      {".txt", "text/plain"},
      {".pdf", "application/pdf"},   {".zip", "application/zip"}};

  // Cari Tanda "." terakhir pada file path
  size_t dot = path.find_last_of('.');
  if (dot != std::string::npos) {
    // Ambil string dari lambang Dot "." tersebut sampai akhir hurup
    std::string ext = path.substr(dot);
    
    // Konversi semua hurup KAPITAL agar jadi Cukup Huruf Kecil yang universal
    // "Gambar.JPG" -> dot. ".jpg"
    for (auto &c : ext)
      c = tolower(c);
      
    // Cocokan huruf dengan tabel statis milik kita di atas
    if (mime_types.count(ext))
      return mime_types[ext]; // jika ditemukan kembalikan teks protokolnya
  }
  
  // Kalau ternyata itu File yang ga kenal/aneh (.exe, .deb, bin). Berikan octet-stream 
  // Browser akan menyuruh si User murni Download file random itu (tidak di rendering di layar Chrome)
  return "application/octet-stream";
}

bool Utils::is_safe_path(const std::string &base_dir,
                         const std::string &target_path,
                         std::string &out_real_path) {
  // Buffer penampung karakter nama path Unix maksium limit (cth: 4096 Karakter)  
  char target_real[PATH_MAX];
  
  // Memakai Fitur Kernel C "realpath()":
  // yang memecahkan ilusi manipulasi file semacam "../" atau "./"  (Relative Link) 
  // menjadi tempat path Asli yang tak tertandingi keakuratannya di dalam hardisk
  if (!realpath(target_path.c_str(), target_real))
    return false; // Jika Kernel lapor file tersebut tak nyata (NotFound/Error Hak Akses), maka di tolak.

  std::string target_str(target_real);
  
  // Kunci Keamanan: Pastikan teks panjang path Asli "target_str" HARUS DIMULAI dengan
  // huruf yang benar-benar sama persis diawali dengan tempat folder "base_dir"
  // (.find mengembalikan lokasi index awal ditemukannya. Index ke-0 "==" bararti valid).
  if (target_str.find(base_dir) == 0) {
    out_real_path = target_str; // Oper data ke wadah kembalian pointer string Asli terverfikasi
    return true; // Sukses! Target aman karena masih terjebak di area kurungan folder www.
  }
  
  // Bahaya, Target mencoba membuka folder yang lolos keluar dari zona base_dir!!
  // Contoh: folder target /var/www  tp klien paksa realpath ke /etc/shadow
  return false;
}
