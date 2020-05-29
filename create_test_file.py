import argparse
import h5py
import numpy as np

def main(args):
    file_params = {'name': args.file, 'mode': 'w'}
    if (args.F):
        file_params.update( {'libver': ('latest', 'latest')} )
    f = h5py.File(**file_params)

    dset_size = 64 * 1024 * 1024
    arr = np.arange(dset_size)

    dset_names = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h']

    for name in dset_names:
        dset_params = {'name': name, 'shape': (dset_size,), 'dtype': 'i8', 'data': arr}

        if (args.c):
            dset_params.update( {'chunks' : (10,)} )

        dset = f.create_dataset(**dset_params)

    f.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Create a test file for H5Dread() profiling.')
    parser.add_argument('-c', action='store_true', help='Create a chunked dataset.')
    parser.add_argument('-F', action='store_true', help='Use the latest file format')
    parser.add_argument('file', help='The name of the file.')
    args = parser.parse_args()
    main(args)

