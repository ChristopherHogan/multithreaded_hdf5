#include <stdio.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Specify file name\n");
    exit(1);
  }

  int num_threads = 1;
  const int max_threads = 8;
  if (argc == 3) {
    num_threads = atoi(argv[2]);
    assert(num_threads <= max_threads);
  }

  const auto now = std::chrono::high_resolution_clock::now;

  char *file_name = argv[1];
  const int dset_size = 64 * 1024 * 1024;

  u64 *a = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *b = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *c = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *d = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *e = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *f = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *g = (u64 *)malloc(dset_size * sizeof(u64));
  u64 *h = (u64 *)malloc(dset_size * sizeof(u64));

  /* hid_t file_id = H5Fopen(file_name, H5F_ACC_RDONLY, H5P_DEFAULT); */
  // fopen

  if (file_id >= 0) {
    fprintf(stderr, "Opened HDF5 file\n");

    const char *dset_names[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    const size_t num_dsets = ARRAY_COUNT(dset_names);
    u64 *destinations[] = {a, b, c, d, e, f, g, h};

    for (size_t i = 0; i < num_dsets; ++i) {
      hid_t dset_id = H5Dopen(file_id, dset_names[i], H5P_DEFAULT);
      if (dset_id > 0) {
        dset_ids[i] = dset_id;
      } else {
        fprintf(stderr, "Failed to open dataset %s\n", dset_names[i]);
      }
    }

    auto start = now();
    auto end = now();

    /* pread64( "", 536870912, 2048) = 536870912 */
    /* pread64( "", 536870912, 3758100480) = 536870912 */
    /* pread64( "", 536870912, 3221229568) = 536870912 */
    /* pread64( "", 536870912, 2684356608) = 536870912 */
    /* pread64( "", 536870912, 2147485696) = 536870912 */
    /* pread64( "", 536870912, 1610614784) = 536870912 */
    /* pread64( "", 536870912, 1073743872) = 536870912 */
    /* pread64( "", 536870912, 536872960) = 536870912 */

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
        int j = i % ARRAY_COUNT(dset_names);
        threads[i] = std::thread(read_dataset, dset_ids[j], dset_names[j], destinations[j]);
        fprintf(stderr, "Queued dataset %s\n", dset_names[j]);
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

    if (H5Fclose(file_id) < 0) {
      fprintf(stderr, "Failed to close file\n");
    }

    fprintf(stderr, "Verifying results\n");
    for (size_t i = 0; i < num_dsets; ++i) {
      u64 counter = 0;
      u64 *current_dset = destinations[i];
      for (int j = 0; j < dset_size; ++j) {
        assert(current_dset[j] == counter++);
      }
    }
  } else {
    fprintf(stderr, "Failed to open file\n");
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
