CXX = g++
PROJ = mth5
BASELINE = mt_posix_io

ifeq ($(DEBUG),1)
	OPT =-O0
	LIB = hdf5_debug
else
	OPT =-O3
	LIB = hdf5
endif

INTEL_ROOT=/opt/intel/vtune_profiler
HDF5root = $(HOME)/local
CXXFLAGS=-ggdb3 $(OPT) -I${HDF5root}/include -I${INTEL_ROOT}/include -Wall -Wextra -pthread
LDFLAGS=-L${HDF5root}/lib -L${INTEL_ROOT}/lib64 -Wl,-rpath,${HDF5root}/lib

all: $(PROJ) $(BASELINE)

$(PROJ): $(PROJ).cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) -l$(LIB) -littnotify -ldl

$(BASELINE): $(BASELINE).cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS) -littnotify -ldl

clean:
	rm -f *.o $(PROJ) $(BASELINE)
