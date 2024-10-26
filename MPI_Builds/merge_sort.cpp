#include <mpi.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

void generateArray(int* data, const std::string& arrType, int arrSize);

int main(int argc, char* argv[]) {
    // Initialize MPI
    MPI_Init(&argc, &argv);

    // Get MPI rank and size
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int num_procs = size;

    // Initialize Adiak
    adiak::init(NULL);

    // Check command-line arguments
    if (argc < 3) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0] << " <total_input_size> <input_type>" << std::endl;
            std::cerr << "Input types: sorted, reverse, random, perturbed" << std::endl;
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    int total_input_size = atoi(argv[1]);
    std::string input_type = argv[2];

    // Application-specific metadata
    std::string algorithm = "merge";
    std::string programming_model = "mpi";
    std::string data_type = "int";
    int size_of_data_type = sizeof(int);
    std::string scalability = "strong"; // Set to "strong" or "weak" depending on the experiment
    int group_number = 16; // Replace with your actual group number
    std::string implementation_source = "handwritten";

    // Collect metadata using Adiak
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();

    adiak::value("algorithm", algorithm);
    adiak::value("programming_model", programming_model);
    adiak::value("data_type", data_type);
    adiak::value("size_of_data_type", size_of_data_type);
    adiak::value("input_size", total_input_size);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", num_procs);
    adiak::value("scalability", scalability);
    adiak::value("group_num", group_number);
    adiak::value("implementation_source", implementation_source);

    // Initialize Caliper ConfigManager
    cali::ConfigManager manager;

    // Start the Caliper manager
    manager.start();

    double start_time = MPI_Wtime();

    // Caliper annotation: begin main
    CALI_MARK_BEGIN("main");

    // Seed the random number generator
    srand(time(NULL) + rank);

    // Compute local input size
    int base_size = total_input_size / size;
    int remainder = total_input_size % size;
    int local_input_size = base_size + (rank < remainder ? 1 : 0);

    // Data initialization (data_init_runtime)
    CALI_MARK_BEGIN("data_init_runtime");

    std::vector<int> local_data(local_input_size);

    // Generate the data using your function
    generateArray(local_data.data(), input_type, local_input_size);

    CALI_MARK_END("data_init_runtime");

    // Computation: local sort (comp_large)
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");

    std::sort(local_data.begin(), local_data.end());

    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    // Now perform parallel merge sort
    int num_steps = std::ceil(std::log2(size));

    for (int step = 0; step < num_steps; ++step) {
        // Compute partner rank
        int partner = rank ^ (1 << step);

        if (partner >= size) {
            continue; // No partner at this step
        }

        // Communication (comm, comm_large)
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");

        // Exchange data sizes
        int local_size = local_data.size();
        int partner_size;
        MPI_Sendrecv(&local_size, 1, MPI_INT, partner, 0,
                     &partner_size, 1, MPI_INT, partner, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        // Exchange data
        std::vector<int> partner_data(partner_size);

        MPI_Sendrecv(local_data.data(), local_size, MPI_INT, partner, 1,
                     partner_data.data(), partner_size, MPI_INT, partner, 1,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");

        // Computation: merge (comp_large)
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");

        std::vector<int> merged_data(local_size + partner_size);
        std::merge(local_data.begin(), local_data.end(),
                   partner_data.begin(), partner_data.end(),
                   merged_data.begin());

        // Determine which half to keep
        if (rank < partner) {
            // Keep the lower half
            merged_data.resize((local_size + partner_size) / 2);
        } else {
            // Keep the upper half
            merged_data.erase(merged_data.begin(),
                              merged_data.begin() + (local_size + partner_size) / 2);
        }

        local_data = std::move(merged_data);

        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        // Synchronize
        MPI_Barrier(MPI_COMM_WORLD);
    }

    // Correctness check (correctness_check)
    CALI_MARK_BEGIN("correctness_check");

    // Gather data to rank 0
    int local_size = local_data.size();
    std::vector<int> recv_counts(size);
    MPI_Gather(&local_size, 1, MPI_INT, recv_counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<int> displs(size, 0);

    if (rank == 0) {
        for (int i = 1; i < size; ++i) {
            displs[i] = displs[i - 1] + recv_counts[i - 1];
        }
    }

    std::vector<int> sorted_data;
    if (rank == 0) {
        sorted_data.resize(total_input_size);
    }

    // Communication (comm, comm_large)
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");

    MPI_Gatherv(local_data.data(), local_size, MPI_INT,
                sorted_data.data(), recv_counts.data(), displs.data(),
                MPI_INT, 0, MPI_COMM_WORLD);

    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    if (rank == 0) {
        // Check if the data is sorted
        bool is_sorted = std::is_sorted(sorted_data.begin(), sorted_data.end());
        if (is_sorted) {
            std::cout << "Data is sorted correctly." << std::endl;
        } else {
            std::cout << "Data is NOT sorted correctly." << std::endl;
        }
    }

    CALI_MARK_END("correctness_check");

    // Caliper annotation: end main
    CALI_MARK_END("main");

    double end_time = MPI_Wtime();
    double total_time = end_time - start_time;

    if (rank == 0) {
        std::cout << "Total execution time: " << total_time << " seconds" << std::endl;
    }

    // Stop and flush the Caliper manager
    manager.flush();
    manager.stop();

    // Finalize MPI
    MPI_Finalize();

    return 0;
}

// Implementation of generateArray
void generateArray(int* data, const std::string& arrType, int arrSize) {
    if (arrType == "sorted") {
        for (int i = 0; i < arrSize; ++i) {
            data[i] = i;
        }
    } else if (arrType == "perturbed") {
        // Generate sorted array
        for (int i = 0; i < arrSize; ++i) {
            data[i] = i;
        }
        // Perturb 1% of the array
        for (int i = 0; i < arrSize / 100; ++i) {
            data[rand() % arrSize] = rand();
        }
    } else if (arrType == "random") {
        for (int i = 0; i < arrSize; ++i) {
            data[i] = rand();
        }
    } else if (arrType == "reverse") {
        for (int i = 0; i < arrSize; ++i) {
            data[i] = arrSize - i;
        }
    } else {
        // Default to random if unknown type
        for (int i = 0; i < arrSize; ++i) {
            data[i] = rand();
        }
    }
}
