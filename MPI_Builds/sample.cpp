#include "mpi.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <numeric>

#include "data.cpp"

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

using namespace std;

bool isSorted(const vector<int>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (arr[i] < arr[i - 1]) {
            return false;
        }
    }
    return true;
}

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

    int initSize = 0;
    string arrType = ""; 

    // Take in command-line arguments
    if (argc == 3) {
        initSize = atoi(argv[1]);
        arrType = argv[2];
    }

    double sort_start, sort_end;

    int rank, num_procs;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "sample"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", to_string(sizeof(int))); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", initSize); // The number of elements in input dataset (1000)
    adiak::value("input_type", arrType); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
    adiak::value("scalability", "weak"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", "16"); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "handwritten"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    vector<int> mainArr;

    // Master process generate initial array
    if (rank == 0) {
        mainArr.resize(initSize);
        CALI_MARK_BEGIN(data_init_runtime);
        generateArray(mainArr.data(), arrType, initSize);
        CALI_MARK_END(data_init_runtime);
        cout << "Sample sort: Array size = " << initSize << " | Array type = " << arrType << " | Num. processors = " << num_procs << endl; 
    }

    int localSize = initSize / num_procs;
    vector<int> localArr(localSize);
    
    // 1. Scatter chunks of initial array to all processes
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    MPI_Scatter(mainArr.data(), localSize, MPI_INT, localArr.data(), localSize, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

    // 2. Locally sort each chunk
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_small);
    sort(localArr.begin(), localArr.end());
    CALI_MARK_END(comp_small);
    CALI_MARK_END(comp);

    // 3. Select pivots
    /* NOTE: 
    - samples.[i * num_procs / (num_procs + 1)] can be changed to get different set of samples.
    - Chose [i * num_procs / (num_procs + 1)] for evenly distributed selection. */

    CALI_MARK_BEGIN(comp);
    vector<int> pivots(num_procs - 1);
    if (rank == 0) {
        vector<int> samples(num_procs);
        for (int i = 0; i < num_procs; ++i) {
            samples[i] = mainArr[i * (initSize / num_procs)];
        }
        CALI_MARK_BEGIN(comp_small);
        sort(samples.begin(), samples.end());
        CALI_MARK_END(comp_small);
        for (int i = 1; i < num_procs; ++i) {
            pivots[i - 1] = samples[i * num_procs / (num_procs + 1)];   //NOTE
        }
    }
    CALI_MARK_END(comp);

    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);
    MPI_Bcast(pivots.data(), num_procs - 1, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

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
    vector<int> sendBuff(accumulate(sendCounts.begin(), sendCounts.end(), 0));
    for (int i = 0; i < num_procs; ++i) {
        copy(buckets[i].begin(), buckets[i].end(), sendBuff.begin() + sendDispls[i]);
    }

    // Calculate counts of elements received for receive buffer 
    vector<int> recvCounts(num_procs);
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_small);
    MPI_Alltoall(sendCounts.data(), 1, MPI_INT, recvCounts.data(), 1, MPI_INT, MPI_COMM_WORLD);
    CALI_MARK_END(comm_small);
    CALI_MARK_END(comm);

    // Calculate disolacement for receive buffer
    vector<int> recvDispls(num_procs);
    recvDispls[0] = 0;
    for (int i = 1; i < num_procs; ++i) {
        recvDispls[i] = recvDispls[i - 1] + recvCounts[i - 1];
    }

    // Prepare receive receive buffer
    vector<int> recvBuff(accumulate(recvCounts.begin(), recvCounts.end(), 0));

    // Exchange send/recv data between all processes
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    MPI_Alltoallv(sendBuff.data(), sendCounts.data(), sendDispls.data(), MPI_INT,
                   recvBuff.data(), recvCounts.data(), recvDispls.data(), MPI_INT, MPI_COMM_WORLD);
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

    // Sort receive buffer 
    CALI_MARK_BEGIN(comp);
    CALI_MARK_BEGIN(comp_small);
    sort(recvBuff.begin(), recvBuff.end());
    CALI_MARK_END(comp_small);
    CALI_MARK_END(comp);

    // Hold sizes of receive buffer from each rank
    vector<int> finalRecvCounts(num_procs);

    // Gather receive buffer sizes from all ranks 
    int recvBuffSize = recvBuff.size();
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    MPI_Gather(&recvBuffSize, 1, MPI_INT, finalRecvCounts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

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
    CALI_MARK_BEGIN(comm);
    CALI_MARK_BEGIN(comm_large);
    MPI_Gatherv(recvBuff.data(), recvBuff.size(), MPI_INT, finalArr.data(), finalRecvCounts.data(), finalRecvDispls.data(), MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END(comm_large);
    CALI_MARK_END(comm);

    if (rank == 0) {
        CALI_MARK_BEGIN(correctness_check);
        bool sorted = isSorted(finalArr);
        if (sorted) {
            cout << "Final array is sorted.";
        } else {
            cout << "Sorted failed: final array not sorted.";
        }
        CALI_MARK_END(correctness_check);
    }

    CALI_MARK_END(main);

    mgr.stop();
    mgr.flush();

    MPI_Finalize();
    return 0;
}
