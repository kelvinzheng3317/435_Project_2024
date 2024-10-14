#include "mpi.h"

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include <string.h>
#include <stdlib.h>
#include <cmath>

#include <algorithm>
#include <iterator>

#include "sort.cpp"

using namespace std;

int main(int argc, char* argv[]) {
  CALI_CXX_MARK_FUNCTION;

  int arrSize = 64;
  string arrType = "sorted"; // sorted, perturbed, random, reverse
  string sortType = "bitonic"; // bitonic, merge, sample, radix

  if (argc == 3) {
    arrSize = atoi(argv[1]);
    arrType = argv[2];
  }

  cali::ConfigManager mgr;
  mgr.start();
  double sort_start, sort_end;

  int procID;
  int num_procs;

  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &procID);
  MPI_Comm_size(MPI_COMM_WORLD,&num_procs);

  int data[arrSize];

  if (procID == 0) {
    // FOR DEBUGGING
    cout << "number of processes: " << num_procs << endl;
    for (int i=0; i<argc; ++i) {
      cout << "arg " << i << " : " << argv[i] << endl;
    }
    
    generateArray(data, arrType, arrSize);
    
    // Prints out starting array for debugging purposes
    for(int i=0; i<arrSize; ++i) {
      cout << data[i] <<", ";
    }
    cout << endl;
    
    CALI_MARK_BEGIN("Bitonic Sort");
    sort_start = MPI_Wtime();
  }


  int local_size = arrSize / num_procs;
  int local_arr[local_size];

  MPI_Scatter(&data, local_size, MPI_INT, &local_arr, local_size, MPI_INT, 0, MPI_COMM_WORLD);

  // BITONIC SORT
  
  // Sorting local array - currently done using alg library
  sort(local_arr, local_arr + local_size);

  int partner_arr[local_size];
  int num_phases = num_procs / 2;
  for (int i=1; i <= num_phases; i *= 2) { // i = first and largest step size of current phase
    // cout << "rank: " << procID << ", i = " << i << endl;
    int procs_per_group = 2 * i;

    // determine if local array is ascending or descending
    bool isAscending = (procID / procs_per_group) % 2 == 0;
    // cout << "rank: " << procID << ", Ascending: " << isAscending << endl;

    int partner_rank;
    int partner_arr[local_size];
    for (int step = i; step > 0; step /= 2) {
      // cout << "rank: " << procID << ", step = " << step << endl;
      int step_group_size = 2 * step;
      if ((procID % step_group_size) < (step_group_size / 2)) {
        partner_rank = procID + step;
      } else {
        partner_rank = procID - step;
      }
      // cout << "rank: " << procID << ", partner rank: " << partner_rank << endl;
        
      // MPI_Sendrecv to send array1 to partner and put partner’s data in array2
      MPI_Sendrecv(&local_arr, local_size, MPI_INT, partner_rank, 0, partner_arr, local_size, MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      int temp[local_size];
      if ((isAscending && procID < partner_rank) || (!isAscending && procID > partner_rank)) {
        // Iterate from left sides of array1 and array2:
        int temp_ind = 0;
        int local_ind = 0;
        int partner_ind = 0;
        while (temp_ind < local_size) {
          // create a temp array of size m with the smallest values from array1 and array2, sorted in increasing order
          if (local_arr[local_ind] <= partner_arr[partner_ind]) {
            temp[temp_ind] = local_arr[local_ind];
            local_ind++;
          } else {
            temp[temp_ind] = partner_arr[partner_ind];
            partner_ind++;
          }
          temp_ind++;
        }
        memcpy(local_arr, temp, local_size * sizeof(int));
      } else {
        // Iterate from the right sides of array1 and array2
        int temp_ind = local_size - 1;
        int local_ind = local_size - 1;
        int partner_ind = local_size - 1;
        while (temp_ind > -1) {
          // create a temp array of size M of the largest values of array1 and array2 in increasing order
          if (local_arr[local_ind] >= partner_arr[partner_ind]) {
            temp[temp_ind] = local_arr[local_ind];
            local_ind--;
          } else {
            temp[temp_ind] = partner_arr[partner_ind];
            partner_ind--;
          }
          temp_ind--;
        }
        memcpy(local_arr, temp, local_size * sizeof(int));
      }
    }
  }

  MPI_Gather(&local_arr, local_size, MPI_INT, &data, local_size, MPI_INT, 0, MPI_COMM_WORLD);


  // print out results to confirm that the array is correctly sorted
  // cout << "Finished sorting" << endl;
  if (procID == 0) {
    CALI_MARK_END("Bitonic Sort");
    sort_end = MPI_Wtime();
    cout << "Sorting time: " << sort_end - sort_start << endl;

    bool correct = true;
    for (int i = 0; i < arrSize - 1; i++) {
      // cout << data[i] << ", ";
      if (data[i] > data[i+1]) {
        correct = false;
        cout << "Array is not correctly sorted" << endl;
        break;
      }
    }
    if (correct) {
      cout << "\n Array is correctly sorted" << endl;
    }
  }

  mgr.stop();
  mgr.flush();

  MPI_Finalize();
}