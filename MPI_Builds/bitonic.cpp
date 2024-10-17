#include "mpi.h"

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include <string.h>
#include <stdlib.h>
#include <cmath>

#include <algorithm>
#include <iterator>

#include "data.cpp"

using namespace std;

int main(int argc, char* argv[]) {
  CALI_CXX_MARK_FUNCTION;

  const char* main = "main";
  const char* data_init_runtime = "data_init_runtime";
  const char* comm = "comm";
  const char* comm_small = "comm_small";
  const char* comm_large = "comm_large";
  const char* comp = "comp";
  const char* comp_small = "comp_small";
  const char* comp_large = "comp_large";
  const char* correctness_check = "correctness_check";

  cali::ConfigManager mgr;
  mgr.start();


  CALI_MARK_BEGIN(main);

  int arrSize = 64;
  string arrType = "sorted"; // sorted, perturbed, random, reverse
  string sortType = "bitonic"; // bitonic, merge, sample, radix

  if (argc == 3) {
    arrSize = atoi(argv[1]);
    arrType = argv[2];
  }

  double sort_start, sort_end;

  int procID;
  int num_procs;

  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &procID);
  MPI_Comm_size(MPI_COMM_WORLD,&num_procs);

  adiak::init(NULL);
  adiak::launchdate();    // launch date of the job
  adiak::libraries();     // Libraries used
  adiak::cmdline();       // Command line used to launch the job
  adiak::clustername();   // Name of the cluster
  adiak::value("algorithm", "bitonic_sort"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
  adiak::value("programming_model", "mpi"); // e.g. "mpi"
  adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
  adiak::value("size_of_data_type", to_string(sizeof(int))); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
  adiak::value("input_size", arrSize); // The number of elements in input dataset (1000)
  adiak::value("input_type", arrType); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
  adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
  adiak::value("scalability", "weak"); // The scalability of your algorithm. choices: ("strong", "weak")
  adiak::value("group_num", "16"); // The number of your group (integer, e.g., 1, 10)
  adiak::value("implementation_source", "handwritten"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
  
  int data[arrSize];

  if (procID == 0) {
    // FOR DEBUGGING
    // cout << "Bitonic sort" << endl;
    // cout << "number of processes: " << num_procs << endl;
    // cout << "array size: " << arrSize << endl;
    // cout << "array type: " << arrType << endl;
    // for (int i=0; i<argc; ++i) {
    //   cout << "arg " << i << " : " << argv[i] << endl;
    // }
    
    CALI_MARK_BEGIN(data_init_runtime);
    generateArray(data, arrType, arrSize);
    CALI_MARK_END(data_init_runtime);

    // Prints out starting array for debugging purposes
    // for(int i=0; i<arrSize; ++i) {
    //   cout << data[i] <<", ";
    // }
    // cout << endl;
  }


  int local_size = arrSize / num_procs;
  int local_arr[local_size];

  CALI_MARK_BEGIN(comm);
  CALI_MARK_BEGIN(comm_large);
  MPI_Scatter(&data, local_size, MPI_INT, &local_arr, local_size, MPI_INT, 0, MPI_COMM_WORLD);
  CALI_MARK_END (comm_large);
  CALI_MARK_END(comm);

  // BITONIC SORT
  
  // Sorting local array - currently done using alg library
  CALI_MARK_BEGIN(comp);
  CALI_MARK_BEGIN(comp_large);
  sort(local_arr, local_arr + local_size);
  CALI_MARK_END(comp_large);
  CALI_MARK_END(comp);

  CALI_MARK_BEGIN(comp);
  int partner_arr[local_size];
  int num_phases = num_procs / 2;
  for (int i=1; i <= num_phases; i *= 2) { // i = first and largest step size of current phase
    CALI_MARK_BEGIN(comp_small);
    int procs_per_group = 2 * i;

    // determine if local array is ascending or descending
    bool isAscending = (procID / procs_per_group) % 2 == 0;

    int partner_rank;
    int partner_arr[local_size];
    CALI_MARK_END(comp_small);

    for (int step = i; step > 0; step /= 2) {
      // cout << "rank: " << procID << ", step = " << step << endl;
      CALI_MARK_BEGIN(comp_small);
      int step_group_size = 2 * step;
      if ((procID % step_group_size) < (step_group_size / 2)) {
        partner_rank = procID + step;
      } else {
        partner_rank = procID - step;
      }
      // cout << "rank: " << procID << ", partner rank: " << partner_rank << endl;
      CALI_MARK_END(comp_small);
        
      // MPI_Sendrecv to send array1 to partner and put partner’s data in array2
      CALI_MARK_BEGIN(comm);
      CALI_MARK_BEGIN(comm_small);
      MPI_Sendrecv(&local_arr, local_size, MPI_INT, partner_rank, 0, partner_arr, local_size, MPI_INT, partner_rank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      CALI_MARK_END(comm_small);
      CALI_MARK_END(comm);

      CALI_MARK_BEGIN(comp_large);
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
      CALI_MARK_END(comp_large);
    }
  }
  CALI_MARK_END(comp);

  CALI_MARK_BEGIN(comm);
  CALI_MARK_BEGIN(comm_large);
  MPI_Gather(&local_arr, local_size, MPI_INT, &data, local_size, MPI_INT, 0, MPI_COMM_WORLD);
  CALI_MARK_END (comm_large);
  CALI_MARK_END(comm);


  // print out results to confirm that the array is correctly sorted
  // cout << "Finished sorting" << endl;
  if (procID == 0) {

    CALI_MARK_BEGIN(correctness_check);
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
    CALI_MARK_END(correctness_check);
  }

  CALI_MARK_END(main);

  mgr.stop();
  mgr.flush();

  MPI_Finalize();
}