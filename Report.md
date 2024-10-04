# CSCE 435 Group project

## 0. Group number: 
Group 16

## 1. Group members:
1. Jaesun Park
2. Kelvin Zheng
3. B.J Min
4. Allen Zhao

Team Communication Method: Discord

## 2. Project topic (e.g., parallel sorting algorithms)

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort:

- Sample Sort:
    Sample sort is a generalization of quicksort designed for parallelized processing. It takes a sample size of s from the original data. It sorts that chosen sample and then divides the sorted sample into p equal-sized groups called 'buckets'. (p is generally chosen as the number of available processors) Then you take p-1 elements from the sorted sample to be 'pivot' values that are used to determine the bucket ranges. Then partition the original data into p buckets with the value in each bucket being in the range between two pivot values. Sort each bucket and then merge them all together. Depending on the size of subarrays, we would either run a simpler sorting algorithm or recursively sample sort them until the remaining subarrays are small enough to be sorted via a simpler sorting algorithm.
    

- Merge Sort: 
    To implement a parallel version of merge sort using MPI, diving the sorting task across multiple processors.
    The idea is for the parent processor to divide the array into x subarrays (depending on how many processors are being used)
    and send them to the workers. The workers will then take that subarray, apply merge sort, and send it back to the master. The master process will then take and merge all the sorted subarrays.

- Radix Sort:

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes

- Sample Sort Pseudocode:
    ```
    MPI_Init()
    int rank
    int num_procs

    procs_elems = length(data) / num_procs   # The number of elements per process
    local_data = empty array of proc_elems
        
    scatter data to processes using MPI_Scatter(local_data)
        
    sorted_local = sample_sort(local_data)

    if rank == 0:    # i.e. MASTER
        sorted_subarrays = empty array
    
    gather sorted local data using MPI_Gather(sorted_local)

    if rank == 0:   # i.e. MASTER
        final_sorted = merge(sorted_subarrays)

    MPI_Finalize()
    ```

- Merge Sort Pseudocode:
    ```Initialize MPI
    int processor_rank // the current process id
    int processor_count // total number of processors currently being used

    if processor_rank == MASTER:
        Divide the main array into subarrays based on 'processor_count'
        for each worker:
            send subarray using MPI_Send

        sortedSubArrays = init empty array
        for each worker:
            receive subarray from worker using MPI_Recv 
            append received subArray into sortedSubArrays
        
        finalSortedArray = merge all the sortedSubArrays here
    
    else: // worker
        Receive subarray from MASTER using MPI_Recv
        Sort the subarray using MergeSort
        Send the sorted array back to MASTER using MPI_Send

    Finalize MPI
    ```

-Bitonic Sort Pseudocode:
    ```
    Int rank
    Int N // number of processes
    MPI_Init // initialize MPI for communication
    MPI_Comm_rank // get the process rank
    MPI_Comm_size // get the number of processes N
    
    If rank == 0:
      Initialize int array of size 2^x where x is an integer
      M = size(array) / N
    
    MPI_Bcast M(the size of local arrays) from rank 0 to all processes
    MPI_Scatter to distribute array data among all processes 

    array1 = array for local data of process
    sort array1

    For (int i=1; i <= N/2; i *= 2): // i = first and largest step size of current phase
      procs_per_group = i * 2
      Ascending if (rank // procs_per_group) % 2 == 0, descending otherwise
      For (int step = i; step > 0; step /= 2):
        if (rank % step) < step / 2:
          partner_rank = rank + step
        else:
          partner_rank = rank - step
          
        MPI_Sendrecv to send array1 to partner and put partner’s data in array2

        If (Ascending and rank <  partner_rank) OR (descending and rank > partner_rank):
          Iterate from left sides of array1 and array2:
            create a temp array of size m with the smallest values from array1 and array2, sorted in increasing order
          Set array1 equal to temp
        Else:
          Iterate from the right sides of array1 and array2:
            Create a temp array of size M of the largest values of array1 and array2 in increasing order
          Set array1 equal to temp

    If rank == 0:
      Merge together all local arrays from all workers using MPI_gather
      Print result array
    ```

### 2c. Evaluation plan - what and how will you measure and compare

### Input Sizes / Input Types
- Varying sizes in order to test the scalability 
    - Example: 10<sup>4</sup>, 10<sup>6</sup>, 10<sup>8</sup>...
- Input types will consist of random data, sorted data, and reversally sorted arrays

### Time:
- Measure the total time taken to sort the array using different number of processors.
- Measure the time between each worker threads assuming that the worker threads are the ones sorting.

### Scalability
#### Strong Scaling
- Maintain the same problem size while increasing the number of processors to measure the time decreasing 

#### Weak Scaling
- Increase the problem size proportionally with the processors to measure the execution time whether it'll be stable or not.
