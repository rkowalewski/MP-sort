#include <mpi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <iomanip>
#include <iostream>

#include <libdash.h>
#include "mpsort.h"

#include "parallel_sort.c"

#define NITER 25
#define BURN_IN 1

#ifdef __cplusplus
extern "C" {
#endif

void mpsort_mpi_report_last_run();

static void radix_int(const void *ptr, void *radix, void *arg)
{
  *(int64_t *)radix = *(const int64_t *)ptr;
}
static int compar_int(const void *p1, const void *p2)
{
  const auto i1 = reinterpret_cast<const int64_t *>(p1);
  const auto i2 = reinterpret_cast<const int64_t *>(p2);
  return (*i1 > *i2) - (*i1 < *i2);
}

#ifdef __cplusplus
}
#endif

int main(int argc, char *argv[])
{
  int i;

  MPI_Init(&argc, &argv);
  dash::init(&argc, &argv);

  int ThisTask = dash::myid();
  int NTask    = dash::size();

  srand(9999 * ThisTask);

  if (argc != 2) {
    printf("./main [number of items]\n");
    return 1;
  }

  size_t mysize  = atoi(argv[1]);
  auto   mydata  = static_cast<int64_t *>(malloc(mysize * sizeof(int64_t)));
  auto   mydata2 = static_cast<int64_t *>(malloc(mysize * sizeof(int64_t)));

  dash::Array<int64_t> mydata3(mysize * NTask);

  if (ThisTask == 0) {
    double mb = (mysize * sizeof(int64_t) / (1 << 20));
    std::cout << "+++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << "++      MP-Sort Benchmark                      ++\n";
    std::cout << "+++++++++++++++++++++++++++++++++++++++++++++++++\n";
    std::cout << std::setw(20) << "NTasks: " << NTask << "\n";
    std::cout << std::setw(20) << "Size per Unit (MB): " << std::fixed
              << std::setprecision(2) << mb;
    std::cout << "\n\n";
    //Print the header
    std::cout << std::setw(4) << "#," << std::setw(15) << "mpsort,"
              << std::setw(15) << "parallel sort," << std::setw(15) << "\n";
  }

  double mpsort_time, psort_time, dsort_time;

  for (size_t iter = 0; iter < NITER + BURN_IN; ++iter) {
    int64_t mysum   = 0;
    int64_t truesum = 0, realsum = 0;
    for (i = 0; i < mysize; i++) {
      uint64_t data =
          (int64_t)random() * (int64_t)random() * random() * random();
      // data = 0 * ThisTask * (int64_t) mysize + i / 10;
      mydata[i]        = data & 0xffffffffffffff;
      mydata2[i]       = mydata[i];
      mydata3.local[i] = mydata[i];
      mysum += mydata[i];
    }

    MPI_Allreduce(
        &mysum, &truesum, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);
    {
      double start = MPI_Wtime();
      mpsort_mpi(
          mydata,
          mysize,
          sizeof(int64_t),
          radix_int,
          sizeof(int64_t),
          NULL,
          MPI_COMM_WORLD);
      MPI_Barrier(MPI_COMM_WORLD);
      double end  = MPI_Wtime();
      mpsort_time = end - start;
    }
    if (ThisTask == 0) {
      // mpsort_mpi_report_last_run();
    }

    {
      double start = MPI_Wtime();
      parallel_sort(mydata2, mysize, sizeof(int64_t), compar_int);
      MPI_Barrier(MPI_COMM_WORLD);
      double end = MPI_Wtime();
      psort_time = end - start;
    }

    {
      double start = MPI_Wtime();
      dash::sort(mydata3.begin(), mydata3.end());
      MPI_Barrier(MPI_COMM_WORLD);
      double end = MPI_Wtime();
      dsort_time = end - start;
    }

    mysum = 0;
    for (i = 0; i < mysize; i++) {
      mysum += mydata[i];
      if (mydata[i] != mydata2[i]) {
        fprintf(stderr, "sorting error\n");
        abort();
      }
    }

    for (i = 0; i < mysize; i++) {
      if (mydata[i] != mydata3.local[i]) {
        fprintf(stderr, "sorting error\n");
        abort();
      }
    }

    MPI_Allreduce(
        &mysum, &realsum, 1, MPI_LONG_LONG, MPI_SUM, MPI_COMM_WORLD);

    if (realsum != truesum) {
      fprintf(stderr, "checksum fail\n");
      MPI_Abort(MPI_COMM_WORLD, 1);
    }
    for (i = 1; i < mysize; i++) {
      if (mydata[i] < mydata[i - 1]) {
        fprintf(stderr, "local ordering fail\n");
      }
    }
    if (NTask > 1) {
      int64_t prev = -1;
      if (ThisTask == 0) {
        if (mysize == 0) {
          MPI_Send(
              &prev, 1, MPI_LONG_LONG, ThisTask + 1, 0xbeef, MPI_COMM_WORLD);
        }
        else {
          MPI_Send(
              &mydata[mysize - 1],
              1,
              MPI_LONG_LONG,
              ThisTask + 1,
              0xbeef,
              MPI_COMM_WORLD);
        }
      }
      else if (ThisTask == NTask - 1) {
        MPI_Recv(
            &prev,
            1,
            MPI_LONG_LONG,
            ThisTask - 1,
            0xbeef,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE);
      }
      else {
        if (mysize == 0) {
          MPI_Recv(
              &prev,
              1,
              MPI_LONG_LONG,
              ThisTask - 1,
              0xbeef,
              MPI_COMM_WORLD,
              MPI_STATUS_IGNORE);
          MPI_Send(
              &prev, 1, MPI_LONG_LONG, ThisTask + 1, 0xbeef, MPI_COMM_WORLD);
        }
        else {
          MPI_Sendrecv(
              &mydata[mysize - 1],
              1,
              MPI_LONG_LONG,
              ThisTask + 1,
              0xbeef,
              &prev,
              1,
              MPI_LONG_LONG,
              ThisTask - 1,
              0xbeef,
              MPI_COMM_WORLD,
              MPI_STATUS_IGNORE);
        }
      }
      if (ThisTask > 1) {
        if (mysize > 0) {
          if (prev > mydata[0]) {
            fprintf(stderr, "global ordering fail\n");
            abort();
          }
        }
      }
    }

    if (iter >= BURN_IN && ThisTask == 0) {
      std::cout << std::setw(3) << iter << "," << std::setw(14) << std::fixed
                << std::setprecision(8) << mpsort_time << "," << std::setw(14)
                << std::fixed << std::setprecision(8) << psort_time << ","
                << std::setw(14) << std::fixed << std::setprecision(8)
                << dsort_time << "\n";
    }
  }

  free(mydata);
  free(mydata2);

  dash::finalize();
  MPI_Finalize();

  return 0;
}
