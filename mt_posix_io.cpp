#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include <chrono>
#include <thread>
#include <vector>

typedef uint32_t u32;
typedef uint64_t u64;

const auto now = std::chrono::high_resolution_clock::now;
const int max_threads = 8;
const int max_dsets = 8;
const int dset_size = 64 * 1024 * 1024;

void create_file(const char *fname, int num_dsets) {
  FILE *file = fopen(fname, "w");
  assert(file);

  std::vector<u64> data(dset_size);
  for (u64 i = 0; i < dset_size; ++i) {
    data[i] = i;
  }

  for (int i = 0; i < num_dsets; ++i) {
    assert(fwrite(data.data(), dset_size * sizeof(u64), 1, file) == 1);
  }
  assert(fclose(file) == 0);
}

void verify_datasets(int num_dsets, int dset_size, u64 **destinations) {
  for (int i = 0; i < num_dsets; ++i) {
    u64 counter = 0;
    u64 *current_dset = destinations[i];
    for (int j = 0; j < dset_size; ++j) {
      assert(current_dset[j] == counter++);
    }
  }
}

void write_datasets(const char *file_name, int num_dsets, int num_threads, bool do_on_worker) {
  size_t dset_bytes = dset_size * sizeof(u64);

  std::vector<u64> data(dset_size);
  for (u64 i = 0; i < dset_size; ++i) {
    data[i] = i;
  }

  FILE *file = fopen(file_name, "w");
  assert(file);

  auto write_func = [&data, dset_bytes](int fd, off_t offset) {
    assert(pwrite(fd, data.data(), dset_bytes, offset) == (int)dset_bytes);
    fprintf(stderr, "Wrote %d of %zu elements\n", dset_size, (size_t)dset_size);
  };

  if (do_on_worker) {
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
      off_t offset = i * dset_bytes;
      threads.push_back(std::thread(write_func, fileno(file), offset));
    }
    for (int i = 0; i < num_threads; ++i) {
      threads[i].join();
    }
  } else {
    FILE *file = fopen(file_name, "w");
    assert(file);
    for (int i = 0; i < num_dsets; ++i) {
      off_t offset = i * dset_bytes;
      write_func(fileno(file), offset);
    }
  }
  assert(fclose(file) == 0);
}

void show_usage_and_exit(const char *prog) {
  fprintf(stderr, "Usage: %s -c file_name\n", prog);
  fprintf(stderr, "       %s -f file_name [-t num_threads] [-d num_dsets] [-rs]\n", prog);
  fprintf(stderr, "       %s -w file_name [-t num_threads] [-d num_dsets] [-as]\n", prog);
  fprintf(stderr, "    -a: Do writes on worker threads\n");
  fprintf(stderr, "    -c: Create a test file called 'file_name'\n");
  fprintf(stderr, "    -r: Read datasets in the worker threads\n");
  fprintf(stderr, "    -s: Skip verification of results\n");
  exit(1);
}

int main (int argc, char* argv[]) {

  if (argc < 2) {
    show_usage_and_exit(argv[0]);
  }

  int option = -1;
  int num_threads = 1;
  int num_dsets = 8;
  char *in_file_name = 0;
  char *out_file_name = 0;
  bool create_test_file = false;
  bool verify_results = true;
  bool do_write = false;
  bool read_on_workers = false;
  bool write_on_workers = false;

  while ((option = getopt(argc, argv, "ac:d:f:rst:w:")) != -1) {
    switch (option) {
      case 'a': {
        write_on_workers = true;
        break;
      }
      case 'c': {
        create_test_file = true;
        out_file_name = optarg;
        break;
      }
      case 'd': {
        num_dsets = atoi(optarg);
        break;
      }
      case 'f': {
        in_file_name = optarg;
        break;
      }
      case 'r': {
        read_on_workers = true;
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
      case 'w': {
        do_write = true;
        out_file_name = optarg;
        break;
      }
      default:
        show_usage_and_exit(argv[0]);
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Got unknown argument '%s'.\n", argv[optind]);
    show_usage_and_exit(argv[0]);
  }

  assert(do_write || create_test_file ? out_file_name : in_file_name);
  assert((num_threads == num_dsets || num_threads == 1 || num_dsets == 1) && "Invalid configuration");
  if (num_dsets == 1) {
    assert(dset_size % num_threads == 0);
  }

  u64 *a = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *b = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *c = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *d = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *e = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *f = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *g = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *h = (u64 *)malloc(dset_size * sizeof(u64));

  u64 *destinations[] = {a, b, c, d, e, f, g, h};
  const int num_ids = num_threads == 1 ? num_dsets : num_threads;
  std::vector<FILE *> dset_ids(num_ids);

  if (create_test_file) {
    create_file(out_file_name, num_dsets);
  } else if (do_write) {
    write_datasets(out_file_name, num_dsets, num_threads, write_on_workers);
  } else {
    for (int i = 0; i < num_ids; ++i) {
      FILE *fid = fopen(in_file_name, "r");
      assert(fid);
      dset_ids[i] = fid;
    }

    auto read_func = [&dset_ids, &destinations](int dset_index, off_t offset) {
      size_t total_size = dset_size * sizeof(u64);
      assert(pread(fileno(dset_ids[dset_index]), destinations[dset_index], total_size,
                   offset) == (int)total_size);
      fprintf(stderr, "Read chunk %d of size %d\n", dset_index, dset_size);
    };

    if (read_on_workers) {
      std::vector<std::thread> threads;
      for (int i = 0; i < num_threads; ++i) {
        off_t offset = i * dset_size * sizeof(u64);
        threads.push_back(std::thread(read_func, i, offset));
      }

      for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
      }
    } else {
      for (int i = 0; i < num_dsets; ++i) {
        read_func(i, i * dset_size * sizeof(u64));
      }
    }

    for (int i = 0; i < num_ids; ++i) {
      assert(fclose(dset_ids[i]) == 0);
    }

  // if (do_write) {
  //   write_datasets(out_file_name, dset_names, num_dsets, num_threads, write_on_workers);
  // } else {
  //   hid_t file_id = H5Fopen(in_file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
  //   assert(file_id >= 0 && "Failed to open file");

  //   // NOTE(chogan): Normally this gets initialized in H5Dopen. Do it here so all
  //   // initialization is complete before starting worker threads.
  //   assert(H5S__init_package() >= 0);
  //   H5S_init_g = 1;

  //   open_datasets(file_id, dset_ids, dset_names, num_dsets, num_threads, open_on_workers);
  //   read_datasets(dset_ids, dset_names, num_dsets, destinations, num_threads, read_on_workers);
  //   close_datasets(dset_ids, dset_names, num_dsets, num_threads, close_on_workers);

  //   if (H5Fclose(file_id) < 0) {
  //     fprintf(stderr, "Failed to close file\n");
  //   }
  // }

  // NOTE(chogan): Stop vtune collection so the computationally expensive
  // verification isn't added to the profile
  // __itt_pause();

  }

  if (verify_results) {
    fprintf(stderr, "Verifying results\n");
    if (do_write) {
      FILE *out_file_id = fopen(out_file_name, "r");
      assert(out_file_id);

      for (int i = 0; i < num_dsets; ++i) {
        size_t dset_bytes = dset_size * sizeof(u64);
        off_t offset = i * dset_bytes;
        assert(pread(fileno(out_file_id), destinations[i], dset_bytes, offset) == (int)dset_bytes);
      }
      assert(fclose(out_file_id) == 0);

      verify_datasets(num_dsets, dset_size, destinations);
      assert(remove(out_file_name) == 0);
    } else {
      verify_datasets(num_dsets, dset_size, destinations);
    }
    fprintf(stderr, "Success.\n");
  }


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
