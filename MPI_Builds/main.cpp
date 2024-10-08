#include "mpi.h"

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include <string.h>
#include <stdlib.h>


int main(int argc, char* argv[]) {
  int arrSize = 64;
  string arrType = "sorted"; // sorted, perturbed, random, reverse
  string sortType = "bitonic" // bitonic, merge, sample, radix

  if (argc == 3) {
    arrSize = atoi(argv[1]);
    arrVals = argv[2];
  }


  int rank;
  int num_procs;

  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&num_procs);

  if (rank == 0) {
    // initialize array based on type given
    int data[arrSize];

    if (arrType == "sorted") {
      for (int i=0; i<arrSize; ++i) {
        data[i] = i;
      }
    } else if (arrType == "perturbed") {
      // should it be exactly 1% of the data or is approximately 1% good enough?
      for (int i=0; i<arrSize; ++i) {
        if (rand() % 100 == 1) {
          data[i] = rand() % 100;
        } else {
          data[i] = i;
        }
      }
    } else if (arrType == "random")
    {
      for (int i = 0; i < arrSize; i++) {
        data[i] = rand() % 100;
      }
    } else if (arrType == "reverse") {
      for (int i=0; i < arrSize; i++) {
        data[i] = arrSize - i;
      }
    }
    
  }

  int local_size = arrSize / num_procs;
  int local_arr[local_size];

  MPI_Scatter(&data, local_size, MPI_INT, &local_arr, local_size, MPI_INT, 0, MPI_COMM_WORLD);

  // Do the sort

  MPI_Gather(local_arr, local_size, MPI_INT, &data, local_size, MPI_INT, 0, MPI_COMM_WORLD);

  // print out results to confirm that the array is correctly sorted
  if (rank == 0) {
    cout << "Sorted array: ";
    for (int i = 0; i < arrSize; i++) {
        cout << data[i] << ", ";
    }
    cout << endl;
  }

  MPI_Finalize();
}