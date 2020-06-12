CXX = g++
PROJ = mth5

ifeq ($(DEBUG),1)
	OPT =-O0
	LIB = hdf5_debug
else
	OPT =-O3
	LIB = hdf5
endif

HDF5root = $(HOME)/local
CXXFLAGS=-ggdb3 $(OPT) -I${HDF5root}/include -Wall -Wextra -pthread
LDFLAGS=-L${HDF5root}/lib -Wl,-rpath,${HDF5root}/lib

all: $(PROJ)

$(PROJ): $(PROJ).cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< -l$(LIB)

clean:
	rm -f *.o $(PROJ)
