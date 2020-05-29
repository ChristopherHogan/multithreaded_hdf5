CXX = g++
SRCS = $(wildcard *.h *.c)
HDRS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)
PROJ = h5dread_mt

HDF5root = ~/local
CXXFLAGS=-ggdb3 -O2 -I${HDF5root}/include -Wall -Wextra -pthread
LDFLAGS=-L${HDF5root}/lib -Wl,-rpath,${HDF5root}/lib

all: $(PROJ)

$(PROJ): h5dread_mt.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< -lhdf5

clean:
	rm -f *.o $(PROJ)
