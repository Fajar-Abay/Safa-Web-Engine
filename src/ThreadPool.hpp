#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/**
 * @class ThreadPool
 * @brief Merupakan mekanisme yang menyimpan sekelompok Thread tetap "hidup"
 *        sehingga menghemat resource prosesor tanpa perlu membuat Utas (thread)
 *        baru setiap ada request masuk.
 */
class ThreadPool {
public:
  /**
   * @brief Konstruktor pembuat Thread Pool
   * @param threads jumlah batas maksimum thread pekerja. Biasanya sama dengan jumlah Core CPU.
   */
  ThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
      // Masukkan sebuah pekerja(worker) yang bentuknya berupa Lambda Function
      // berulang (infinite loop) yang tugasnya cuma menunggu antrian pekerjaan
      workers.emplace_back([this] {
        for (;;) {
          std::function<void()> task;
          {
            // Lock Mutex: Mencegah 2 thread berebut tiket tugas yang sama dari antrian
            std::unique_lock<std::mutex> lock(this->queue_mutex);
            
            // Condition Wait: TIDUR (tidak pakai 1% pun CPU) sampai antrian tak kosong
            // atau sampai waktu pemberhentian (stop=true)
            this->condition.wait(
                lock, [this] { return this->stop || !this->tasks.empty(); });
                
            // Jika instruksi stop ditarik, Thread ini harus diakhiri secara damai (return)
            if (this->stop && this->tasks.empty())
              return;
              
            // Ambil pekerjaan paling di depan dari antrean (queue FIFO)
            task = std::move(this->tasks.front());
            this->tasks.pop();
          }
          // Eksekusi pekerjaan tersebut
          task();
        }
      });
    }
  }

  /**
   * @brief Pintu pelempar Tugas (Enqueue)
   * Menyodorkan sebuah function/lambda closure ke dalam antrian kerja thread pool.
   */
  template <class F> void enqueue(F &&f) {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      tasks.emplace(std::forward<F>(f)); // Memasukkan function request handler
    }
    // Bunyikan bel (notify_one) untuk membangunkan 1 dari sekian worker yang lagi tertidur
    condition.notify_one();
  }

  /**
   * @brief Destruktor (Shutdown bersih)
   * Mengatur alarm pemberhentian dan menunggu semua pekerja selesai mencetak tugas terakhirnya.
   */
  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      stop = true; // Angkat tanda bendera putih untuk tutup warung
    }
    condition.notify_all(); // Bangunin SEMUA thread yang tidur
    for (std::thread &worker : workers)
      worker.join(); // Gabungkan (tunggu) semua pekerja menyudahi putarannya sebelum mati
  }

private:
  std::vector<std::thread> workers;          // Menyimpan kumpulan utas (Thread) yang nyata
  std::queue<std::function<void()>> tasks;   // Antrian berbentuk Fungsi Kosong (Job Queue)
  std::mutex queue_mutex;                    // Kunci penjaga loket antrian
  std::condition_variable condition;         // Alat penjadwal (Bel Pemberitahuan)
  bool stop;                                 // Lampu isyarat jalan/berhenti
};

#endif
