#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "hdf5.h"
#include "ittnotify.h"


#define ARRAY_COUNT(arr) (sizeof((arr)) / sizeof((arr)[0]))

typedef uint32_t u32;
typedef uint64_t u64;

herr_t H5S__init_package();
extern int H5S_init_g;
const auto now = std::chrono::high_resolution_clock::now;
const int max_threads = 8;

// struct Barrier {
//   int wait_count;
//   int target_wait_count;
//   std::mutex mtx;
//   std::condition_variable cond_var;

//   Barrier() {};

//   void wait() {
//     std::unique_lock<std::mutex> lk(mtx);
//     ++wait_count;
//     if(wait_count != target_wait_count) {
//       // not all threads have arrived yet; go to sleep until they do
//       cond_var.wait(lk, [this]() { return wait_count == target_wait_count; });
//     } else {
//       // we are the last thread to arrive; wake the others and go on
//       cond_var.notify_all();
//     }
//     // note that if you want to reuse the barrier, you will have to
//     // reset wait_count to 0 now before calling wait again
//     // if you do this, be aware that the reset must be synchronized with
//     // threads that are still stuck in the wait
//   }
// };

void read_dataset(hid_t dset_id, const char *dset_name, void *dest) {
  assert(H5Dread(dset_id, H5T_STD_I64LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, dest) >= 0);
  fprintf(stderr, "Read dataset %s\n", dset_name);
}

double read_on_workers(hid_t file_id, const char **dset_names, std::vector<hid_t> &dset_ids,
                       int num_threads, u64 **destinations) {
  size_t num_dsets = dset_ids.size();
  auto start = now();
  for (size_t i = 0; i < num_dsets; ++i) {
    hid_t dset_id = H5Dopen(file_id, dset_names[i], H5P_DEFAULT);
    if (dset_id > 0) {
      dset_ids[i] = dset_id;
    } else {
      fprintf(stderr, "Failed to open dataset %s\n", dset_names[i]);
    }
  }
  auto end = now();
  fprintf(stderr, "Time for sequential opens: %f s.\n",
          std::chrono::duration<double>(end - start).count());

  start = now();
  if (num_threads == 1) {
    start = now();
    for (size_t i = 0; i < num_dsets; ++i) {
      read_dataset(dset_ids[i], dset_names[i], destinations[i]);
    }
    end = now();
  } else {
    std::thread threads[max_threads];
    start = now();
    for (int i = 1; i < num_threads; ++i) {
      threads[i] = std::thread(read_dataset, dset_ids[i], dset_names[i], destinations[i]);
      fprintf(stderr, "Queued dataset %s\n", dset_names[i]);
    }

    fprintf(stderr, "Queued dataset %s\n", dset_names[0]);
    read_dataset(dset_ids[0], dset_names[0], destinations[0]);
    for (int i = 1; i < num_threads; ++i) {
      threads[i].join();
    }
    end = now();
  }
  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to read 8 datasets with %d threads: %f\n", num_threads, total_seconds);
  printf("%f\n", total_seconds);

  for (size_t i = 0; i < num_dsets; ++i) {
    if (H5Dclose(dset_ids[i]) < 0){
      fprintf(stderr, "Failed to close dataset %s\n", dset_names[i]);
    }
  }

  return total_seconds;
}

void open_dataset(hid_t file_id, const char *dset_name, hid_t *dset_id) {
  hid_t id = H5Dopen(file_id, dset_name, H5P_DEFAULT);
  assert(id >= 0);
  *dset_id = id;
  fprintf(stdout, "Opened %s\n", dset_name);
}

double open_and_read_on_workers(hid_t file_id, const char **dset_names, int num_threads,
                                u64 **destinations) {
  std::vector<std::thread> open_threads;
  hid_t dset_ids[max_threads] = {};

  fprintf(stderr, "Opens\n");
  auto start = now();
  for (int i = 0; i < num_threads; ++i) {
    open_threads.push_back(std::thread(open_dataset, file_id, dset_names[i], &dset_ids[i]));
  }

  for (auto &t : open_threads) {
    t.join();
  }
  auto end = now();
  fprintf(stderr, "Parallel opens: %f s.\n",std::chrono::duration<double>(end - start).count());
  std::vector<std::thread> read_threads;

  fprintf(stderr, "Reads\n");
  start = now();
  for (int i = 0; i < num_threads; ++i) {
    read_threads.push_back(std::thread(read_dataset, dset_ids[i], dset_names[i], destinations[i]));
  }

  for (auto &t : read_threads) {
    t.join();
  }

  end = now();
  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to read 8 datasets with %d threads: %f\n", num_threads, total_seconds);
  printf("%f\n", total_seconds);

  for (int i = 0; i < num_threads; ++i) {
    if (H5Dclose(dset_ids[i]) < 0){
      fprintf(stderr, "Failed to close dataset %s\n", dset_names[i]);
    }
  }

  return total_seconds;
}

void close_dataset(hid_t dset_id, const char *name) {
  if (H5Dclose(dset_id) < 0) {
    fprintf(stderr, "Failed to close dataset %s\n", name);
  } else {
    fprintf(stdout, "Closed %s\n", name);
  }
}

double open_read_and_close_on_workers(hid_t file_id, const char **dset_names, int num_threads,
                                      u64 **destinations) {
  std::vector<std::thread> open_threads;
  hid_t dset_ids[max_threads] = {};

  fprintf(stderr, "Opens\n");
  auto start = now();
  for (int i = 0; i < num_threads; ++i) {
    open_threads.push_back(std::thread(open_dataset, file_id, dset_names[i], &dset_ids[i]));
  }

  for (auto &t : open_threads) {
    t.join();
  }
  auto end = now();
  fprintf(stderr, "Parallel opens: %f s.\n",std::chrono::duration<double>(end - start).count());
  std::vector<std::thread> read_threads;

  fprintf(stderr, "Reads\n");
  start = now();
  for (int i = 0; i < num_threads; ++i) {
    read_threads.push_back(std::thread(read_dataset, dset_ids[i], dset_names[i], destinations[i]));
  }

  for (auto &t : read_threads) {
    t.join();
  }

  end = now();
  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to read 8 datasets with %d threads: %f\n", num_threads, total_seconds);
  printf("%f\n", total_seconds);

  std::vector<std::thread> close_threads;

  fprintf(stderr, "Closes\n");
  start = now();
  for (int i = 0; i < num_threads; ++i) {
    close_threads.push_back(std::thread(close_dataset, dset_ids[i], dset_names[i]));
  }

  for (auto &t : close_threads) {
    t.join();
  }
  end = now();
  fprintf(stderr, "Parallel closes: %f s.\n",std::chrono::duration<double>(end - start).count());

  return total_seconds;
}

void usage(const char *prog) {
  fprintf(stderr, "Usage: %s -f file_name [-t num_threads] [-o,-c,-s]\n", prog);
  fprintf(stderr, "    -o: Open datasets in the worker threads\n");
  fprintf(stderr, "    -c: Close datasets in the worker threads\n");
  fprintf(stderr, "    -s: Skip verification of results\n");
  exit(1);
}

const u32 open_on_workers = 0x1;
const u32 close_on_workers = 0x2;

int main (int argc, char* argv[]) {

  if (argc < 2) {
    usage(argv[0]);
  }

  int option = -1;
  int num_threads = 1;
  char *file_name = 0;
  u32 work_split = 0;
  bool verify_results = true;

  while ((option = getopt(argc, argv, "cf:ost:")) != -1) {
    switch (option) {
      case 'c': {
        work_split |= close_on_workers;
        break;
      }
      case 'f': {
        file_name = optarg;
        break;
      }
      case 'o': {
        work_split |= open_on_workers;
        break;
      }
      case 's': {
        verify_results = false;
        break;
      }
      case 't': {
        num_threads = atoi(optarg);
        assert(num_threads <= max_threads);
        break;
      }
      default:
        usage(argv[0]);
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Got unknown argument '%s'.\n", argv[optind]);
    usage(argv[0]);
  }

  const int dset_size = 64 * 1024 * 1024;

  u64 *a = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *b = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *c = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *d = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *e = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *f = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *g = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *h = (u64 *)malloc(dset_size * sizeof(u64));

  hid_t file_id = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
  assert(file_id >= 0 && "Failed to open file");
  // NOTE(chogan): Normally this gets initialized in H5Dopen. Do it here so all
  // initialization is complete before starting worker threads.
  assert(H5S__init_package() >= 0);
  H5S_init_g = 1;

  const char *dset_names[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
  const size_t num_dsets = ARRAY_COUNT(dset_names);
  u64 *destinations[] = {a, b, c, d, e, f, g, h};
  std::vector<hid_t> dset_ids(num_dsets);

  if ((work_split & open_on_workers) && (work_split & close_on_workers)) {
    open_read_and_close_on_workers(file_id, dset_names, num_threads, destinations);
  } else if (work_split & open_on_workers) {
    open_and_read_on_workers(file_id, dset_names, num_threads, destinations);
  } else {
    read_on_workers(file_id, dset_names, dset_ids, num_threads, destinations);
  }

  if (H5Fclose(file_id) < 0) {
    fprintf(stderr, "Failed to close file\n");
  }

  __itt_pause();

  if (verify_results) {
    fprintf(stderr, "Verifying results\n");
    for (int i = 0; i < num_threads; ++i) {
      u64 counter = 0;
      u64 *current_dset = destinations[i];
      for (int j = 0; j < dset_size; ++j) {
        assert(current_dset[j] == counter++);
      }
    }
  }
  fprintf(stderr, "Success.\n");

  free(a);
  free(b);
  free(c);
  free(d);
  free(e);
  free(f);
  free(g);
  free(h);

  return 0;
}
