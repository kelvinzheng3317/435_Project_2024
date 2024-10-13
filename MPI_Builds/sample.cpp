#include "mpi.h"
#include <vector>
#include <algorithm>
#include <iostream>

// For data generation or randomized array
#include <cstdlib>
#include <ctime>

// For performance analysis, not used currently
#include <caliper/cali.h>
#include <caliper/cali-manager.h>

using std::vector;
using std::cout, std::endl;
using std::sort;

void printArray(const vector<int>& arr, int rank, const std::string& step) {
    cout  << "Rank" << rank << " " << step << ": ";
    for (int elem : arr) {
        cout << num << " ";
    }
    cout << endl;
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int arrSize = 16;
    vector<int> mainArr;

    if (rank == 0) {
        std::rand(std::time(0));
        mainArr.resize(arrSize);
        for (int i = 0; i < arrSize; ++i) {
            mainArr[i] = std::rand() % 100;    // Fill array with numbers between 0 and 99
        }
        printArray(mainArr, rank, "Initial Array");
    }

    
    return 0;
}