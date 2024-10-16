#include "mpi.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <numeric>

#include "data.cpp"

using namespace std;

// // Function to print array
// void printArray(const vector<int>& arr, int rank, const string& step) {
//     cout << "Rank " << rank << " - " << step << ": ";
//     for (int elem : arr) {
//         cout << elem << " ";
//     }
//     cout << endl;
// }

bool isSorted(const vector<int>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i] < arr[i - 1]) {
            return false;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    int initSize = 0;
    string arrType = ""; 

    // Take in command-line arguments
    if (argc == 3) {
        initSize = atoi(argv[1]);
        arrType = argv[2];
    }

    int rank, num_procs;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    vector<int> mainArr;

    // Master process generate initial array
    if (rank == 0) {
        mainArr.resize(initSize);
        generateArray(mainArr.data(), arrType, initSize);
        cout << "Sample sort: Array size = " << initSize << " | Array type = " << arrType << " | Num. processors = " << num_procs << endl; 
    }

    int localSize = initSize / num_procs;
    vector<int> localArr(localSize);
    
    // 1. Scatter chunks of initial array to all processes
    MPI_Scatter(mainArr.data(), localSize, MPI_INT, localArr.data(), localSize, MPI_INT, 0, MPI_COMM_WORLD);

    // 2. Locally sort each chunk
    sort(localArr.begin(), localArr.end());

    // 3. Select pivots
    /* NOTE: 
    - samples.[i * num_procs / (num_procs + 1)] can be changed to get different set of samples.
    - Chose [i * num_procs / (num_procs + 1)] for evenly distributed selection. */

    vector<int> pivots(num_procs - 1);
    if (rank == 0) {
        vector<int> samples(num_procs);
        for (int i = 0; i < num_procs; ++i) {
            samples[i] = mainArr[i * (initSize / num_procs)];
        }
        sort(samples.begin(), samples.end());
        for (int i = 1; i < num_procs; ++i) {
            pivots[i - 1] = samples[i * num_procs / (num_procs + 1)];   //NOTE
        }
    }

    MPI_Bcast(pivots.data(), num_procs - 1, MPI_INT, 0, MPI_COMM_WORLD);

    // 4. Bucketing based on pivots
    vector<vector<int>> buckets(num_procs);
    for (int i = 0; i < localSize; ++i) {
        int bucket_no = 0;
        while (bucket_no < num_procs - 1 && localArr[i] > pivots[bucket_no]) {
            ++bucket_no;
        }
        buckets[bucket_no].push_back(localArr[i]);
    }

    // 5. Calculate necessary values for send/recv buffers
    // Calculate counts of elements for sending buffer from each bucket
    vector<int> sendCounts(num_procs);
    for (int i = 0; i < num_procs; ++i) {
        sendCounts[i] = buckets[i].size();
    }

    // Calculate displacement for sending buffer
    vector<int> sendDispls(num_procs);
    sendDispls[0] = 0;
    for (int i = 1; i < num_procs; ++i) {
        sendDispls[i] = sendDispls[i - 1] + sendCounts[i - 1];
    }

    // Prepare send buffer
    vector<int> sendBuffer(accumulate(sendCounts.begin(), sendCounts.end(), 0));
    for (int i = 0; i < num_procs; ++i) {
        copy(buckets[i].begin(), buckets[i].end(), sendBuffer.begin() + sendDispls[i]);
    }

    // Calculate counts of elements received for receive buffer 
    vector<int> recvCounts(num_procs);
    MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);

    // Calculate disolacement for receive buffer
    vector<int> recvDispls(num_procs);
    recvDispls[0] = 0;
    for (int i = 1; i < num_procs; ++i) {
        recvDispls[i] = recvDispls[i - 1] + recvCounts[i - 1];
    }

    // Prepare receive receive buffer
    vector<int> recvBuffer(accumulate(recvCounts.begin(), recvCounts.end(), 0));

    // Exchange send/recv data between all processes
    MPI_Alltoallv(sendBuffer.data(), sendCounts.data(), sendDispls.data(), MPI_INT,
                   recvBuffer.data(), recvCounts.data(), recvDispls.data(), MPI_INT, MPI_COMM_WORLD);

    // Sort receive buffer locally
    sort(recvBuffer.begin(), recvBuffer.end());

    // Hold sizes of receive buffer from each rank
    vector<int> finalRecvCounts(num_procs);

    // Gather receive buffer sizes from all ranks 
    int recvBufferSize = recvBuffer.size();
    MPI_Gather(&recvBufferSize, 1, MPI_INT, finalRecvCounts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calculate displacements for final receive buffers
    vector<int> finalRecvDispls(num_procs);
    if (rank == 0) {
        finalRecvDispls[0] = 0;
        for (int i = 1; i < num_procs; ++i) {
            finalRecvDispls[i] = finalRecvDispls[i - 1] + finalRecvCounts[i - 1];
        }
    }

    // 6. Gather all sorted received buffers into final array
    vector<int> finalArr(initSize);
    MPI_Gatherv(recvBuffer.data(), recvBuffer.size(), MPI_INT, finalArr.data(), finalRecvCounts.data(), finalRecvDispls.data(), MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        bool sorted = isSorted(finalArr);
        if (sorted) {
            cout << "Final array is sorted.";
        } else {
            cout << "Sorted failed: final array not sorted.";
        }
    }

    MPI_Finalize();
    return 0;
}
