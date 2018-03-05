CC=cc
MPICC=mpicc
CFLAGS=-std=c99 -O3 -DNDEBUG

CXXFLAGS=-std=c++11 -O3 -DNDEBUG
DASHROOT=$(HOME)/opt/dash-0.3.0
MPICXX= $(DASHROOT)/bin/dash-mpic++ -dash:verbose

PREFIX=$(HOME)/opt/mpsort

all: libradixsort.a libmpsort-mpi.a

install: libradixsort.a libmpsort-mpi.a
	install -d $(PREFIX)/lib
	install -d $(PREFIX)/include
	install libradixsort.a $(PREFIX)/lib/libradixsort.a
	install libmpsort-mpi.a $(PREFIX)/lib/libmpsort-mpi.a
	install mpsort.h $(PREFIX)/include/mpsort.h

clean:
	rm -f *.o *.a main-mpi bench-mpi main bench-dash
tests: main main-mpi bench-mpi

main: main.c libmpsort-omp.a libradixsort.a
	$(CC) $(CFLAGS) -o main $^
main-mpi: main-mpi.c libmpsort-mpi.a libradixsort.a
	$(MPICC) $(CFLAGS) -o main-mpi $^
bench-mpi: bench-mpi.c libmpsort-mpi.a libradixsort.a
	$(MPICC) $(CFLAGS) -o bench-mpi $^
bench-dash: bench-dash.cc libmpsort-mpi.a libradixsort.a
	$(MPICXX) $(CXXFLAGS) -o bench-dash $^

libradixsort.a: radixsort.c
	$(CC)  $(CFLAGS)  -c -o radixsort.o radixsort.c
	ar r libradixsort.a radixsort.o
	ranlib libradixsort.a

libmpsort-omp.a: mpsort-omp.c
	$(CC)  $(CFLAGS) -c -o mpsort-omp.o mpsort-omp.c
	ar r libmpsort-omp.a mpsort-omp.o
	ranlib libmpsort-omp.a

libmpsort-mpi.a: mpsort-mpi.c
	$(MPICC)  $(CFLAGS) -c -o mpsort-mpi.o mpsort-mpi.c
	ar r libmpsort-mpi.a mpsort-mpi.o
	ranlib libmpsort-mpi.a

