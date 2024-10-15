#include "mpi.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdlib>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include "sort.cpp" 

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

    vector<int> data;

    // main process
    if(processID == 0){
        data.resize(arraySize);
        generateArray(data.data(), arrType, arraySize);

        CALI_MARK_BEGIN("Merge Sort");
        sort_start = MPI_Wtime();
    }

    // distribute data to all processes
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

    CALI_MARK_BEGIN("Local Merge Sort");
    mergeSort(localData, 0, localSize - 1);
    CALI_MARK_END("Local Merge Sort");

    if (processID == 0) {
        // Master process collects sorted subarrays from workers
        vector<int> sortedData = localData;
        for (int i = 1; i < numProcesses; i++) {
            // Receive the size of the sorted subarray
            int recvSize;
            MPI_Status status;
            MPI_Recv(&recvSize, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

            // Receive the sorted subarray
            vector<int> recvData(recvSize);
            MPI_Recv(&recvData[0], recvSize, MPI_INT, i, 0, MPI_COMM_WORLD, &status);

            // Merge the received subarray with the main sorted data
            mergeVectors(sortedData, recvData);
        }

        sort_end = MPI_Wtime();
        CALI_MARK_END("Merge Sort");
        cout << "Sorting time: " << sort_end - sort_start << " seconds" << endl;

        // Verify that the array is correctly sorted
        bool correct = is_sorted(sortedData.begin(), sortedData.end());
        if (correct) {
            cout << "Array is correctly sorted" << endl;
        } else {
            cout << "Array is not correctly sorted" << endl;
        }

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