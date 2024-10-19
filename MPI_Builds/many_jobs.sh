#!/bin/bash

# Define the parameter ranges
# input_sizes=(65536 262144 1048576 4194304 16777216 67108864 268435456)  # 2^16, 2^18, ..., 2^28
input_sizes=(64 128)
# input_types=("sorted" "random" "reverse" "perturbed")
input_types=("sorted" "random" "reverse" "perturbed")
# num_procs=(2 4 8 16 32 64 128 256 512 1024)
num_procs=(32 64)

# Define the sorting algorithm (can be replaced if needed)
sort_alg="bitonic"  # Replace with the actual sorting algorithm executable

# Path to the mpi.grace_job file
job_file="mpi.grace_job"

# Loop over all combinations of input sizes, input types, and number of processors
for input_size in "${input_sizes[@]}"; do
    for input_type in "${input_types[@]}"; do
        for procs in "${num_procs[@]}"; do
            # Calculate the number of nodes and tasks per node
            tasks_per_node=32
            nodes=$(( (procs + tasks_per_node - 1) / tasks_per_node ))  # Ceiling division for nodes
            if [ $procs -lt 32 ]; then
                tasks_per_node=$procs
            else
                tasks_per_node=32
            fi

            # Print out for tracking
            echo "Submitting job with: input_size=$input_size, input_type=$input_type, procs=$procs, nodes=$nodes, tasks_per_node=$tasks_per_node"

            # Create a temporary job file with updated parameters
            tmp_job_file="tmp_grace_job_${input_size}_${input_type}_${procs}.sh"

            # Copy the original job file to the temporary job file and replace nodes and ntasks-per-node
            sed "s/#SBATCH --nodes=.*/#SBATCH --nodes=$nodes/; \
                 s/#SBATCH --ntasks-per-node=.*/#SBATCH --ntasks-per-node=$tasks_per_node/" $job_file > $tmp_job_file

            # Submit the job using sbatch
            sbatch $tmp_job_file $input_size $input_type $sort_alg $procs

            # Check for success and delay (optional) between job submissions
            if [ $? -eq 0 ]; then
                echo "Job submitted successfully!"
            else
                echo "Job submission failed!"
            fi

            # Remove the temporary job file after submission
            rm $tmp_job_file

            # Optional sleep to avoid overwhelming the scheduler
            # sleep 1
        done
    done
done
