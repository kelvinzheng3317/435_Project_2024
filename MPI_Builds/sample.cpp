#include "mpi.h"
#include <vector>
#include <algorithm>
#include <iostream>

// For performance analysis, not used currently
#include <caliper/cali.h>
#include <caliper/cali-manager.h>

using std::vector;
using std::cout, std::endl;
using std::sort;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int arr_size = 16;
    vector<int> main_arr;

    MPI
    return 0;
}