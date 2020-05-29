#!/bin/bash

set -x

# Build hdf5
git clone https://chogan@bitbucket.hdfgroup.org/scm/~chogan/hdf5.git
pushd hdf5
git checkout chogan/mt_h5dread

if [[ -d build ]]; then
    rm -rf build
fi

mkdir build
pushd build

if [[ ! -z "${DEBUG}" ]]; then
    BUILD_TYPE=-DCMAKE_BUILD_TYPE=Debug
    DBG_FLAGS="-ggdb3 -O0"
else
    BUILD_TYPE=-DCMAKE_BUILD_TYPE=Release
    DBG_FLAGS="-g"
fi

LOCAL=${HOME}/local

CFLAGS="${DBG_FLAGS} -DNDEBUG"            \
  cmake                                   \
    -DCMAKE_INSTALL_PREFIX=${LOCAL}       \
    -DCMAKE_PREFIX_PATH=${LOCAL}          \
    -DCMAKE_BUILD_RPATH=${LOCAL}/lib      \
    -DCMAKE_INSTALL_RPATH=${LOCAL}/lib    \
    -DBUILD_SHARED_LIBS=ON                \
    -DBUILD_STATIC_LIBS=OFF               \
    -DHDF5_BUILD_CPP_LIB=OFF              \
    -DHDF5_BUILD_HL_LIB=OFF               \
    -DHDF5_ENABLE_THREADSAFE=ON           \
    -DHDF5_ENABLE_USING_MEMCHECKER=ON     \
    ${BUILD_TYPE}                         \
    ..

cmake --build . -- -j 6 VERBOSE=1 && make install

popd
popd
