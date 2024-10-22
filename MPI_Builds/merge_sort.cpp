#include "mpi.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdlib>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include "data.cpp" 

using namespace std;

/**
 * @brief Merges two vectors
 * 
 * @param local_data the local data
 * @param recv_data the received data
 */
void mergeVectors(vector<int>& local_data, const vector<int>& recv_data) {
    CALI_CXX_MARK_FUNCTION;
    vector<int> temp(local_data.size() + recv_data.size());
    std::merge(local_data.begin(), local_data.end(), recv_data.begin(), recv_data.end(), temp.begin());
    local_data = move(temp);
}

/**
 * @brief applies the merge sort algorithm to the data
 * 
 * @param data the data to be sorted
 * @param left the left index
 * @param right the right index
 */
void mergeSort(vector<int>& data, int left, int right) {
    CALI_CXX_MARK_FUNCTION;
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSort(data, left, mid);
        mergeSort(data, mid + 1, right);
        vector<int> temp(right - left + 1);
        std::merge(data.begin() + left, data.begin() + mid + 1,
                   data.begin() + mid + 1, data.begin() + right + 1,
                   temp.begin());
        copy(temp.begin(), temp.end(), data.begin() + left);
    }
}


int main(int argc, char* argv[]){
    CALI_CXX_MARK_FUNCTION;

    // metadata
    adiak::init(NULL);
    string algorithm = "merge";
    string programming_model = "mpi";
    string data_type = "int";
    int size_of_data_type = sizeof(int);
    int num_procs;
    string scalability = "strong"; // change this as needed
    int group_number = 19; // adjust based on your group
    string implementation_source = "handwritten";

    int arraySize = 64; // default array size
    string arrType = "sorted"; // default array type
    string sortType = "merge"; // default sort type (bitonic, merge, sample, radix)

    // parse command line arguments
    if(argc == 3){
        arraySize = atoi(argv[1]);
        arrType = argv[2];
    }

    cali::ConfigManager manager;
    MPI_Init(&argc, &argv);
    manager.start();

    double sort_start, sort_end;

    int processID;
    int numProcesses;

    MPI_Comm_rank(MPI_COMM_WORLD, &processID);
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);

    num_procs = numProcesses; // Assign the correct number of processes

    // Collect
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", algorithm); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", programming_model); // e.g. "mpi"
    adiak::value("data_type", data_type); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", size_of_data_type); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", arraySize); // The number of elements in input dataset (1000)
    adiak::value("input_type", arrType); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
    adiak::value("scalability", scalability); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", group_number); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", implementation_source); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    vector<int> data;

    // main process
    if(processID == 0){
        CALI_MARK_BEGIN("data_init_runtime");
        data.resize(arraySize);
        generateArray(data.data(), arrType, arraySize);
        CALI_MARK_END("data_init_runtime");
    }

    // Begin Merge sort timing
    if(processID == 0){
        sort_start = MPI_Wtime();
    }

    // distribute data to all processes
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    int baseSize = arraySize / numProcesses;
    int remainder = arraySize % numProcesses;
    int localSize = baseSize + (processID < remainder ? 1 : 0);

    vector<int> localData(localSize);

    if(processID == 0){
        int offset = 0;
        for(int i = 0; i < numProcesses; i++){
            int sendSize = baseSize + (i < remainder ? 1 : 0);
            if(i == 0){
                copy(data.begin(), data.begin() + sendSize, localData.begin());
                offset += sendSize;
            } else {
                MPI_Send(&data[offset], sendSize, MPI_INT, i, 0, MPI_COMM_WORLD);
                offset += sendSize;
            }
        }
    }else{
        // receiving end
        MPI_Recv(&localData[0], localSize, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    mergeSort(localData, 0, localSize - 1);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    if (processID == 0) {
        CALI_MARK_BEGIN("comm");
        // Master process collects sorted subarrays from workers
        vector<int> sortedData = localData;
        for (int i = 1; i < numProcesses; i++) {
            CALI_MARK_BEGIN("comm_large");
            // Receive the size of the sorted subarray
            int recvSize;
            MPI_Status status;
            MPI_Recv(&recvSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

            // Receive the sorted subarray
            vector<int> recvData(recvSize);
            MPI_Recv(&recvData[0], recvSize, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

            // Merge the received subarray with the main sorted data
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_small");
            mergeVectors(sortedData, recvData);
            CALI_MARK_END("comp_small");
            CALI_MARK_END("comp");

            CALI_MARK_END("comm_large");
        }
        CALI_MARK_END("comm");

        sort_end = MPI_Wtime();

        CALI_MARK_BEGIN("correctness_check");
        cout << "Sorting time: " << sort_end - sort_start << " seconds" << endl;
        // Verify that the array is correctly sorted
        bool correct = is_sorted(sortedData.begin(), sortedData.end());
        if (correct) {
            cout << "Array is correctly sorted" << endl;
        } else {
            cout << "Array is not correctly sorted" << endl;
        }
        CALI_MARK_END("correctness_check");

    } else {
        // Worker processes send their sorted subarrays back to the master
        int sendSize = localData.size();
        MPI_Send(&sendSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        MPI_Send(&localData[0], sendSize, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    manager.stop();
    manager.flush();

    MPI_Finalize();
    return 0;
}