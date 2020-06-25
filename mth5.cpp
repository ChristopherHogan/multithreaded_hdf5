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

extern int H5S_init_g;
herr_t H5S__init_package();

typedef uint32_t u32;
typedef uint64_t u64;

const auto now = std::chrono::high_resolution_clock::now;
const int max_threads = 8;
const int max_dsets = 8;
const int dset_size = 64 * 1024 * 1024;

void open_datasets(hid_t file_id, std::vector<hid_t> &dset_ids, const char **dset_names,
                   int num_dsets, int num_threads, bool do_on_worker) {
  std::vector<std::thread> threads;

  auto open_func = [file_id, dset_names, &dset_ids](int name_index, int id_index) {
    hid_t id = H5Dopen(file_id, dset_names[name_index], H5P_DEFAULT);
    assert(id >= 0);
    dset_ids[id_index] = id;
    fprintf(stderr, "Opened %s\n", dset_names[name_index]);
  };

  auto start = now();
  auto end = now();

  if (do_on_worker) {
    if (num_threads != (int)num_dsets) {
      assert(num_dsets == 1);
    }
    start = now();
    for (int i = 0; i < num_threads; ++i) {
      // NOTE(chogan): If we only have 1 dataset, then we open the first name,
      // otherwise we open the ith name
      int name_index = num_dsets == 1 ? 0 : i;
      threads.push_back(std::thread(open_func, name_index, i));
    }

    for (int i = 0; i < num_threads; ++i) {
      threads[i].join();
    }
    end = now();
  } else {
    start = now();
    for (int i = 0; i < num_dsets; ++i) {
      open_func(i, i);
    }
    end = now();
  }
  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to open %d datasets with %d threads: %f\n", num_dsets,
          do_on_worker ? num_threads : 1, total_seconds);
}

void write_datasets(const char *file_name, const char **dset_names, int num_dsets, int num_threads,
                    bool do_on_worker) {
  std::vector<std::thread> threads;

  hid_t file_id = H5Fcreate(file_name, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  assert(file_id >= 0);

  const hsize_t dset_size = 64 * 1024 * 1024;
  hid_t dspace = H5Screate_simple(1, &dset_size, NULL);
  assert(dspace >= 0);

  std::vector<hid_t> dset_ids;
  for (int i = 0; i < num_dsets; ++i) {
    hid_t dataset_id = H5Dcreate(file_id, dset_names[i], H5T_NATIVE_ULONG, dspace,
                                 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    assert(dataset_id >= 0);
    dset_ids.push_back(dataset_id);
  }

  std::vector<u64> data(dset_size);
  for (size_t i = 0; i < dset_size; ++i) {
    data[i] = i;
  }

  auto write_func = [&data, &dset_ids, dset_names](int dset_index, int name_index, hid_t mspace_id,
                                                   hid_t fspace_id, void *buf, int elems_written) {
    assert(H5Dwrite(dset_ids[dset_index], H5T_NATIVE_ULONG, mspace_id, fspace_id, H5P_DEFAULT,
                    buf) >= 0);
    fprintf(stderr, "Wrote %d of %zu elements to dataset %s\n", elems_written, (size_t)dset_size,
            dset_names[name_index]);
    assert(H5Dclose(dset_ids[dset_index]) <= 0);
  };

  auto start = now();
  auto end = now();

  if (do_on_worker) {
    if (num_threads != num_dsets) {
      assert(num_dsets == 1);
      // NOTE(chogan): Main thread sets up dataspaces for partial writes
      std::vector<hid_t> dspaces;
      hsize_t offset = 0;
      const hsize_t stride = 1;
      const hsize_t count = dset_size / num_threads;
      const hsize_t block = 1;

      hid_t mspace = H5Screate_simple(1, &count, NULL);
      assert(mspace >= 0);

      for (int i = 0; i < num_threads; ++i) {
        hid_t dspace_id = H5Dget_space(dset_ids[i]);
        assert(dspace >= 0);
        assert(H5Sselect_hyperslab(dspace_id, H5S_SELECT_SET, &offset, &stride, &count, &block) >= 0);
        dspaces.push_back(dspace);

        hssize_t dspace_elems = H5Sget_select_npoints(dspace_id);
        hssize_t mspace_elems = H5Sget_select_npoints(mspace);
        assert(dspace_elems == mspace_elems);

        offset += count;
      }
      // NOTE(chogan): Each thread writes num_elems / num_threads elements
      start = now();
      for (int i = 0; i < num_threads; ++i) {
        size_t data_offset = i * count;
        int name_index = num_dsets == 1 ? 0 : i;
        threads.push_back(std::thread(write_func, i, name_index, mspace, dspaces[i],
                                      data.data() + data_offset, count));
      }

      for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
      }
      end = now();

      for (int i = 0; i < num_threads; ++i) {
        assert(H5Sclose(dspaces[i]) >= 0);
      }
      assert(H5Sclose(mspace) >= 0);
    } else {
      // NOTE(chogan): Each thread writes a whole dataset
      start = now();
      for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread(write_func, i, i, H5S_ALL, H5S_ALL, data.data(), dset_size));
      }

      for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
      }
      end = now();
    }
  } else {
    start = now();
    for (int i = 0; i < num_dsets; ++i) {
      write_func(i, i, H5S_ALL, H5S_ALL, data.data(), dset_size);
    }
    end = now();
  }

  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to write %d datasets with %d threads: %f\n", num_dsets,
          do_on_worker ? num_threads : 1, total_seconds);

  assert(H5Sclose(dspace) >= 0);
  assert(H5Fclose(file_id) >= 0);
}

void read_datasets(const std::vector<hid_t> &dset_ids, const char **dset_names, int num_dsets,
                   u64 **dests, int num_threads, bool do_on_worker) {
  std::vector<std::thread> threads;

  auto read_func = [&dset_ids, &dset_names](int dset_index, int name_index, hid_t mspace_id,
                                            hid_t fspace_id, u64 *dest, int elems_read) {
    assert(H5Dread(dset_ids[dset_index], H5T_STD_I64LE, mspace_id, fspace_id, H5P_DEFAULT, dest) >= 0);
    fprintf(stderr, "Read %zu of %d elements from dataset %s\n", (size_t)elems_read, dset_size,
            dset_names[name_index]);
  };

  auto start = now();
  auto end = now();

  if (do_on_worker) {
    if (num_threads != (int)num_dsets) {
      assert(num_dsets == 1);
      // NOTE(chogan): Main thread sets up dataspaces for partial reads
      std::vector<hid_t> dspaces;
      hsize_t offset = 0;
      const hsize_t stride = 1;
      const hsize_t count = dset_size / num_threads;
      const hsize_t block = 1;

      hid_t mspace = H5Screate_simple(1, &count, NULL);
      assert(mspace >= 0);

      for (int i = 0; i < num_threads; ++i) {
        hid_t dspace = H5Dget_space(dset_ids[i]);
        assert(dspace >= 0);
        assert(H5Sselect_hyperslab(dspace, H5S_SELECT_SET, &offset, &stride, &count, &block) >= 0);
        dspaces.push_back(dspace);

        hssize_t dspace_elems = H5Sget_select_npoints(dspace);
        hssize_t mspace_elems = H5Sget_select_npoints(mspace);
        assert(dspace_elems == mspace_elems);

        offset += count;
      }

      // NOTE(chogan): Each thread reads num_elems / num_threads elements
      start = now();
      for (int i = 0; i < num_threads; ++i) {
        size_t dest_offset = i * count;
        int name_index = num_dsets == 1 ? 0 : i;
        threads.push_back(std::thread(read_func, i, name_index, mspace, dspaces[i],
                                      dests[0] + dest_offset, count));
      }

      for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
      }
      end = now();

      for (int i = 0; i < num_threads; ++i) {
        assert(H5Sclose(dspaces[i]) >= 0);
      }
      assert(H5Sclose(mspace) >= 0);
    } else {
      // NOTE(chogan): Each thread reads a whole dataset
      start = now();
      for (int i = 0; i < num_threads; ++i) {
        threads.push_back(std::thread(read_func, i, i, H5S_ALL, H5S_ALL, dests[i], dset_size));
      }

      for (int i = 0; i < num_threads; ++i) {
        threads[i].join();
      }
      end = now();
    }
  } else {
    // NOTE(chogan): One thread reads all datasets
    start = now();
    for (int i = 0; i < num_dsets; ++i) {
      read_func(i, i, H5S_ALL, H5S_ALL, dests[i], dset_size);
    }
    end = now();
  }

  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to read %d datasets with %d threads: %f\n", num_dsets,
          do_on_worker ? num_threads : 1, total_seconds);
}

void close_datasets(const std::vector<hid_t> &dset_ids, const char **dset_names, int num_dsets,
                    int num_threads, bool do_on_worker) {
  std::vector<std::thread> threads;

  auto close_func = [&dset_ids, &dset_names](int dset_index, int name_index) {
    assert(H5Dclose(dset_ids[dset_index]) >= 0);
    fprintf(stderr, "Closed %s\n", dset_names[name_index]);
  };

  auto start = now();
  auto end = now();

  if (do_on_worker) {
    if (num_threads != num_dsets) {
      assert(num_dsets == 1);
    }

    start = now();
    for (int i = 0; i < num_threads; ++i) {
      int name_index = num_dsets == 1 ? 0 : i;
      threads.push_back(std::thread(close_func, i, name_index));
    }

    for (int i = 0; i < num_threads; ++i) {
      threads[i].join();
    }
    end = now();
  } else {
    start = now();
    for (size_t i = 0; i < dset_ids.size(); ++i) {
      int name_index = num_dsets == 1 ? 0 : i;
      close_func(i, name_index);
    }
    end = now();
  }

  double total_seconds = std::chrono::duration<double>(end - start).count();
  fprintf(stderr, "Total seconds to close %d datasets with %d threads: %f\n", num_dsets,
          do_on_worker ? num_threads : 1, total_seconds);
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
  char *in_file_name = 0;
  char *out_file_name = 0;
  bool verify_results = true;
  bool do_write = false;
  bool open_on_workers = false;
  bool read_on_workers = false;
  bool write_on_workers = false;
  bool close_on_workers = false;

  while ((option = getopt(argc, argv, "acd:f:orst:w:")) != -1) {
    switch (option) {
      case 'a': {
        write_on_workers = true;
        break;
      }
      case 'c': {
        close_on_workers = true;
        break;
      }
      case 'd': {
        num_dsets = atoi(optarg);
        assert(num_dsets <= max_dsets);
        break;
      }
      case 'f': {
        in_file_name = optarg;
        break;
      }
      case 'o': {
        open_on_workers = true;
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
        usage(argv[0]);
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Got unknown argument '%s'.\n", argv[optind]);
    usage(argv[0]);
  }

  assert(do_write ? out_file_name : in_file_name);
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

  const char *dset_names[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
  u64 *destinations[] = {a, b, c, d, e, f, g, h};
  const int num_ids = num_threads == 1 ? num_dsets : num_threads;
  std::vector<hid_t> dset_ids(num_ids);

  if (do_write) {
    write_datasets(out_file_name, dset_names, num_dsets, num_threads, write_on_workers);
  } else {
    hid_t file_id = H5Fopen(in_file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
    assert(file_id >= 0 && "Failed to open file");

    // NOTE(chogan): Normally this gets initialized in H5Dopen. Do it here so all
    // initialization is complete before starting worker threads.
    assert(H5S__init_package() >= 0);
    H5S_init_g = 1;

    open_datasets(file_id, dset_ids, dset_names, num_dsets, num_threads, open_on_workers);
    read_datasets(dset_ids, dset_names, num_dsets, destinations, num_threads, read_on_workers);
    close_datasets(dset_ids, dset_names, num_dsets, num_threads, close_on_workers);

    if (H5Fclose(file_id) < 0) {
      fprintf(stderr, "Failed to close file\n");
    }
  }

  // NOTE(chogan): Stop vtune collection so the computationally expensive
  // verification isn't added to the profile
  __itt_pause();

  if (verify_results) {
    fprintf(stderr, "Verifying results\n");
    if (do_write) {
      hid_t out_file_id = H5Fopen(out_file_name, H5F_ACC_RDONLY, H5P_DEFAULT);
      assert(out_file_id >= 0);

      for (int i = 0; i < num_dsets; ++i) {
        hid_t dset_id = H5Dopen(out_file_id, dset_names[i], H5P_DEFAULT);
        assert(dset_id >= 0);
        assert(H5Dread(dset_id, H5T_NATIVE_ULONG, H5S_ALL, H5S_ALL, H5P_DEFAULT,
                       destinations[i]) >= 0);
        assert(H5Dclose(dset_id) >= 0);
      }

      verify_datasets(num_dsets, dset_size, destinations);

      assert(H5Fclose(out_file_id) >= 0);
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
