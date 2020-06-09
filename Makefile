CXX = g++
PROJ = mth5

HDF5root = $(HOME)/local
CXXFLAGS=-ggdb3 -O2 -I${HDF5root}/include -Wall -Wextra -pthread
LDFLAGS=-L${HDF5root}/lib -Wl,-rpath,${HDF5root}/lib

all: $(PROJ)

$(PROJ): mth5.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< -lhdf5

clean:
	rm -f *.o $(PROJ)
