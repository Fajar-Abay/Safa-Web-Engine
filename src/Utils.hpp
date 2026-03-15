#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

/**
 * @class Utils
 * @brief Memuat fungsi alat murni pembantu teknis komputasi kecil-kecilan.
 *
 * Di kelas ini, tidak ada penyimpanan status (Variabel member / Property OOP),
 * Melainkan murni sebagai penyedia logika fungsional abstrak untuk
 * file Sistem maupun Konversi tipe data string ke ekstensi MIME type.
 */
class Utils {
public:
  /**
   * @brief Mendeteksi Ekstensi Format Data MIME (Media Type)
   * Mengambil file path ".png/.html", mengembalikan String Text Format "image/png"
   * yang dipakai peramban web Google/Firefox agar mau meload gambar tersebut
   * tanpa menampilkan teks acak hancur sebagai huruf biner kacau.
   *
   * @param path target alamat file utuh seperti "/style.css"
   * @return std::string nama pengembang protokol (contoh "text/css")
   */
  static std::string get_mime_type(const std::string &path);

  /**
   * @brief Pintu Pengaman (Penjaga Serangan Path Traversal LFI)
   * Menyaring permintaan jahat "hacker" yang menyisipkan titik garis (/../../../etc/passwd)
   *
   * @param base_dir Letak Tembok Folder Asli server kita (contoh: ./www)
   * @param target_path Yang di minta si malang (contoh ./www/../etc/passwd)
   * @param out_real_path Hasil Modifikasi Kernel Linux letak sebenarnya path yang diminta  
   * @return true Jika File Valid dan TIDAK MENYINGGUR di atas folder induk aman (base_dir)
   *         false Jika menembus/tidak sah atau folder ga ada.
   */
  static bool is_safe_path(const std::string &base_dir,
                           const std::string &target_path,
                           std::string &out_real_path);
};

#endif
