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

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    const int initSize = 16;
    vector<int> mainArr;

    if (rank == 0) {
        std::rand(std::time(0));
        mainArr.resize(initSize);
        for (int i = 0; i < initSize; ++i) {
            mainArr[i] = std::rand() % 100;    // Fill array with numbers between 0 and 99
        }
        printArray(mainArr, rank, "Initial Array");
    }

    int localSize = initSize / num_procs;
    vector<int> localArr(localSize);
    
    // 1. Scatter chunks of main array to all processes
    MPI_Scatter(mainArr.data(), localSize, MPI_INT, localArr.data(), localSize, MPI_INT, 0, MPI_COMM);
    printArray(localArray, rank, "Received chunk");

    // 2. Locally sort each chunk
    sort(localArr.begin(), localArr.end());
    printArray(localArr, rank, "Local Sorted Chunk");

    
    // 3. Select pivots
    // NOTE: samples.[i * num_procs / (num_procs + 1)] can be changed to get different set of samples
    // Chose [i * num_procs / (num_procs + 1)] for evenlt distributed selection

    vector<int> pivots(num_procs - 1);
    if (rank == 0) {
        vector<int> samples;
        for (int i = 0; i < num_procs; ++i) {
            samples.push_back(mainArr[i * (initSize / num_procs)]);
        }
        sort(samples.begin(), samples.end());
        for (int i = 1; i < num_procs; ++i) {
            pivots[i - 1] = samples.[i * num_procs / (num_procs + 1)];
        }
    }
    MPI_Bcast(pivots.data(), num_procs - 1, MPI_INT, 0, MPI_COMM_WORLD);
    printArray(pivots, rank, "Pivots");

    // 4. Bucketing based on pivots

    return 0;
}