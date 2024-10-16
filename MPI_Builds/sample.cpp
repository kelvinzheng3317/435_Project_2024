#include "mpi.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <numeric>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

// For data generation or randomized array
#include <cstdlib>
#include <ctime>

#include "data.cpp"

using namespace std;

void printArray(const vector<int>& arr, int rank, const string& step) {
    cout  << "Rank " << rank << " - " << step << ": ";
    for (int elem : arr) {
        cout << elem << " ";
    }
    cout << endl;
}

int main(int argc, char* argv[]) {
    CALI_CXX_MARK_FUNCTION;

    int initSize = 64;
    string arrType = "random"; // sorted, perturbed, random, reverse
    string sortType = "sample"; // bitonic, merge, sample, radix

    if (argc == 3) {
        initSize = atoi(argv[1]);
        arrType = argv[2];
    }

    cali::ConfigManager mgr;
    mgr.start();
    
    double sort_start, sort_end;

    int rank, num_procs;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    vector<int> mainArr;
    
    if (rank == 0) {
        mainArr.resize(initSize);
        generateArray(mainArr.data(), arrType, initSize);
        printArray(mainArr, rank, "Initial Array");

        CALI_MARK_BEGIN("Sample Sort");
        sort_start = MPI_Wtime();
    }

    int localSize = initSize / num_procs;
    vector<int> localArr(localSize);
    
    // 1. Scatter chunks of main array to all processes
    MPI_Scatter(mainArr.data(), localSize, MPI_INT, localArr.data(), localSize, MPI_INT, 0, MPI_COMM_WORLD);

    // 2. Locally sort each chunk
    sort(localArr.begin(), localArr.end());
    
    // 3. Select pivots
    /* NOTE: samples.[i * num_procs / (num_procs + 1)] can be changed to get different set of samples.
    Chose [i * num_procs / (num_procs + 1)] for evenlt distributed selection. */

    vector<int> pivots(num_procs - 1);
    if (rank == 0) {
        vector<int> samples;
        for (int i = 0; i < num_procs; ++i) {
            samples.push_back(mainArr[i * (initSize / num_procs)]);
        }
        sort(samples.begin(), samples.end());
        for (int i = 1; i < num_procs; ++i) {
            pivots[i - 1] = samples[i * num_procs / (num_procs + 1)]; //NOTE
        }
    }
    MPI_Bcast(pivots.data(), num_procs - 1, MPI_INT, 0, MPI_COMM_WORLD);

    // 4. Bucketing based on pivots
    vector<vector<int>> buckets(num_procs);
    for (int i = 0; i < localSize; ++i) {
        int bucket_no = 0;
        while ((bucket_no < num_procs - 1) && (localArr[i] > pivots[bucket_no])) {
            ++bucket_no;
        }
        buckets[bucket_no].push_back(localArr[i]);
    }

    // 5. Prepare send and receive buffers for bucket data
    vector<int> sendCounts(num_procs), recvCounts(num_procs);
    for (int i = 0; i < num_procs; ++i) {
        sendCounts[i] = buckets[i].size();
    }
    MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);

    // Calculating displacements for for send/recv
    vector<int> sendDispls(num_procs), recvDispls(num_procs);
    partial_sum(sendCounts.begin(), sendCounts.end() - 1, sendDispls.begin() + 1);
    partial_sum(recvCounts.begin(), recvCounts.end() - 1, recvDispls.begin() + 1);

    // Flatten buckets into single send buffer
    vector<int> sendBuffer(accumulate(sendCounts.begin(), sendCounts.end(), 0));
    for (int i = 0; i < num_procs; ++i) {
        copy(buckets[i].begin(), buckets[i].end(), sendBuffer.begin() + sendDispls[i]);
    }

    // Receive buffer for the redistributed data
    vector<int> recvBuffer(accumulate(recvCounts.begin(), recvCounts.end(), 0));

    // 6. Redistribute the data to all processes
    MPI_Alltoallv(sendBuffer.data(), sendCounts.data(), sendDispls.data(), MPI_INT,
                recvBuffer.data(), recvCounts.data(), recvDispls.data(), MPI_INT, MPI_COMM_WORLD);

    // 7. Sort all received buckets
    sort(recvBuffer.begin(), recvBuffer.end());

    // 8. Gather sorted received subarrays (buckets) to MASTER process and print
    vector<int> finalArr;
    if (rank == 0) {
        finalArr.resize(initSize);
    }
    MPI_Gather(recvBuffer.data(), recvBuffer.size(), MPI_INT, finalArr.data(), recvBuffer.size(), MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        sort_end = MPI_Wtime();
        CALI_MARK_END("Sample Sort");

        cout << "Final sorted array: ";
        for (int i = 0; i < initSize; ++i) {
            cout << finalArr[i] << " ";
        }
        cout << endl;
        cout  << "Total sorting time: " << sort_end - sort_start << "secs" << endl;
    }

    MPI_Finalize();
    return 0;
}
