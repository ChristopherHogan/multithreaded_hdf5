#!/bin/bash

module load python
conda create  -n h5py h5py
source activate h5py
python create_test_file.py -F test.h5
source deactivate
conda env remove -n h5py
module unload python

