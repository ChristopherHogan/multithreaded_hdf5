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
const int max_dsets = 8;
const int dset_size = 64 * 1024 * 1024;

// Flags for assigning work
const u32 open_on_workers = 0x1;
const u32 read_on_workers = 0x2;
const u32 close_on_workers = 0x4;


void open_datasets(hid_t file_id, std::vector<hid_t> &dset_ids, const char **dset_names,
                   int num_threads, bool do_on_worker) {
  std::vector<std::thread> threads;
  const size_t num_dsets = dset_ids.size();

  auto open_func = [file_id, dset_names, &dset_ids](int dset_index) {
    hid_t id = H5Dopen(file_id, dset_names[dset_index], H5P_DEFAULT);
    assert(id >= 0);
    dset_ids[dset_index] = id;
    fprintf(stdout, "Opened %s\n", dset_names[dset_index]);
  };

  auto start = now();
  auto end = now();

  if (do_on_worker) {
    start = now();
    for (int i = 0; i < num_threads; ++i) {
      threads.push_back(std::thread(open_func, i));
    }

    for (int i = 0; i < num_threads; ++i) {
      threads[i].join();
    }
    end = now();
  } else {
    start = now();
    for (size_t i = 0; i < num_dsets; ++i) {
      open_func(i);
    }
    end = now();
  }
  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to open %zu datasets with %d threads: %f\n", num_dsets, num_threads,
          total_seconds);
}

void read_datasets(const std::vector<hid_t> &dset_ids, const char **dset_names,
                   u64 **dests, int num_threads, bool do_on_worker) {
  std::vector<std::thread> threads;
  const size_t num_dsets = dset_ids.size();

  auto read_func = [&dset_ids, &dset_names, &dests](int dset_index) {
    assert(H5Dread(dset_ids[dset_index], H5T_STD_I64LE, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                   dests[dset_index]) >= 0);
    fprintf(stderr, "Read dataset %s\n", dset_names[dset_index]);
  };

  auto start = now();
  auto end = now();

  if (do_on_worker) {
    if (num_threads != (int)num_dsets) {
      assert(num_dsets == 1);
      start = now();
      // TODO(chogan): Need offset and length for each read
      end = now();
    } else {
      // Each thread reads a whole dataset
      start = now();
      for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread(read_func, i));
      }

      for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
      }
      end = now();
    }
  } else {
    start = now();
    for (size_t i = 0; i < num_dsets; ++i) {
      read_func(i);
    }
    end = now();
  }

  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to read %zu datasets with %d threads: %f\n", num_dsets, num_threads,
          total_seconds);
}

void close_datasets(const std::vector<hid_t> &dset_ids, const char **dset_names, int num_threads,
                    bool do_on_worker) {
  std::vector<std::thread> threads;
  const size_t num_dsets = dset_ids.size();

  auto close_func = [&dset_ids, &dset_names](int dset_index) {
    assert(H5Dclose(dset_ids[dset_index]) >= 0);
    fprintf(stdout, "Closed %s\n", dset_names[dset_index]);
  };

  auto start = now();
  auto end = now();

  if (do_on_worker) {
    start = now();
    for (int i = 0; i < num_threads; ++i) {
      threads.push_back(std::thread(close_func, i));
    }

    for (int i = 0; i < num_threads; ++i) {
      threads[i].join();
    }
    end = now();
  } else {
    start = now();
    for (size_t i = 0; i < num_dsets; ++i) {
      close_func(i);
    }
    end = now();
  }

  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to close %zu datasets with %d threads: %f\n", num_dsets, num_threads,
          total_seconds);
}

void usage(const char *prog) {
  fprintf(stderr, "Usage: %s -f file_name [-t num_threads] [-d num_dsets] [-o,-c,-r,-s]\n", prog);
  fprintf(stderr, "    -c: Close datasets in the worker threads\n");
  fprintf(stderr, "    -o: Open datasets in the worker threads\n");
  fprintf(stderr, "    -r: Read datasets in the worker threads\n");
  fprintf(stderr, "    -s: Skip verification of results\n");
  exit(1);
}

int main (int argc, char* argv[]) {

  if (argc < 2) {
    usage(argv[0]);
  }

  int option = -1;
  int num_threads = 1;
  int num_dsets = 8;
  char *file_name = 0;
  u32 work_split = 0;
  bool verify_results = true;

  while ((option = getopt(argc, argv, "cd:f:orst:")) != -1) {
    switch (option) {
      case 'c': {
        work_split |= close_on_workers;
        break;
      }
      case 'd': {
        num_dsets = atoi(optarg);
        assert(num_dsets <= max_dsets);
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
      case 'r': {
        work_split |= read_on_workers;
        break;
      }
      case 's': {
        verify_results = false;
        break;
      }
      case 't': {
        num_threads = atoi(optarg);
        assert(num_threads <= max_dsets);
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

  assert((num_threads == num_dsets || num_threads == 1 || num_dsets == 1) && "Invalid configuration");

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
  u64 *destinations[] = {a, b, c, d, e, f, g, h};
  std::vector<hid_t> dset_ids(num_dsets);

  open_datasets(file_id, dset_ids, dset_names, num_threads, work_split & open_on_workers);
  read_datasets(dset_ids, dset_names, destinations, num_threads, work_split & read_on_workers);
  close_datasets(dset_ids, dset_names, num_threads, work_split & close_on_workers);

  if (H5Fclose(file_id) < 0) {
    fprintf(stderr, "Failed to close file\n");
  }

  // __itt_pause();

  if (verify_results) {
    fprintf(stderr, "Verifying results\n");
    for (int i = 0; i < num_dsets; ++i) {
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
