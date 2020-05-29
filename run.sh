#!/bin/bash


amplxe-cli -collect threading -result-dir cori_no_locks ./h5dread_mt test.h5 8

pushd hdf5
git checkout HEAD~1
pushd build
make -j 6 && make install
popd
popd

make -B

amplxe-cli -collect threading -result-dir cori_lock_side_calls ./h5dread_mt test.h5 8

pushd hdf5
git checkout develop
pushd build
make -j 6 && make install
popd
popd

make -B

amplxe-cli -collect threading -result-dir cori_develop ./h5dread_mt test.h5 8
