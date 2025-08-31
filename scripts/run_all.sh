#!/bin/bash

# Define the variable configurations
NCPUS=(1 2 4 8 16 32)
CHUNKSIZES=(4 8 16 32 64 256 1024)
MTX_FILE="datasets/large_diameter/matrices_list.txt"
N=32
PAPI_EVENTS="PAPI_L3_TCA,PAPI_L3_TCM"

# Create a directory for the logs if it doesn't exist
mkdir -p logs

# Generate a single timestamp for this script run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "Starting benchmark run. Logs will be saved in the 'logs/' directory."
echo "-------------------------------------------"

mapfile -t mtx_files < "$MTX_FILE"

# # --- Pthreads Execution ---
# echo "Running Pthreads benchmarks..."
# for ncpus in "${NCPUS[@]}"; do
#   for dataset in "${mtx_files[@]}"; do
#     for chunksize in "${CHUNKSIZES[@]}"; do
#       dataset_basename=$(basename "$dataset")
#       LOG_FILE="logs/pthreads ${dataset_basename} ${ncpus}cpus chunk${chunksize} ${TIMESTAMP}.log"
      
#       echo "  - Pthreads: CPUs=$ncpus, Dataset=$dataset, Chunksize=$chunksize -> ${LOG_FILE}"
#       # Write header to log
#       echo "$ncpus, $dataset, $chunksize" > "$LOG_FILE"
#       # Execute and append stdout/stderr to log
#       ./pthreads/targets/bfs_chunksize${chunksize}_${ncpus}cpus -f "${dataset}" -n ${N} >> "$LOG_FILE" 2>&1
#     done
#   done
# done

# echo "Pthreads benchmarks complete."
# echo "-------------------------------------------"


# # --- OpenMP Execution ---
# echo "Running OpenMP benchmarks..."
# for ncpus in "${NCPUS[@]}"; do
#   for dataset in "${mtx_files[@]}"; do
#     dataset_basename=$(basename "$dataset")
#     LOG_FILE="logs/openmp ${dataset_basename} ${ncpus}cpus ${TIMESTAMP}.log"

#     echo "  - OpenMP: CPUs=$ncpus, Dataset=$dataset -> ${LOG_FILE}"
#     # Write header to log (chunksize is 0)
#     echo "$ncpus, $dataset, 0" > "$LOG_FILE"
#     # Execute and append stdout/stderr to log
#     OMP_NUM_THREADS=${ncpus} openmp/targets/bfs "${dataset}" ${N} merged_csr_distances >> "$LOG_FILE" 2>&1
#   done
# done
# echo "OpenMP benchmarks complete."
# echo "-------------------------------------------"


# # --- GAP-BS Execution ---
# echo "Running GAP-BS benchmarks..."
# for ncpus in "${NCPUS[@]}"; do
#   for dataset in "${mtx_files[@]}"; do
#     dataset_basename=$(basename "$dataset")
#     LOG_FILE="logs/gapbs ${dataset_basename} ${ncpus}cpus ${TIMESTAMP}.log"

#     echo "  - GAP-BS: CPUs=$ncpus, Dataset=$dataset -> ${LOG_FILE}"
#     # Write header to log (chunksize is 0)
#     echo "$ncpus, $dataset, 0" > "$LOG_FILE"
#     # Execute and append stdout/stderr to log
#     OMP_NUM_THREADS=${ncpus} gapbs/targets/bfs -f "${dataset}" -n ${N} >> "$LOG_FILE" 2>&1
#   done
# done

# echo "GAP-BS benchmarks complete."
# echo "-------------------------------------------"
# echo "All benchmarks finished."

# --- Execution Phase ---
echo "Starting execution with PAPI..."
mkdir -p papi

# Read matrix files into an array
# mapfile -t mtx_files < "$MTX_FILE"
mtx_files=("datasets/large_diameter/europe_osm.mtx")

for mtx in "${mtx_files[@]}"; do
    mtx_basename=$(basename "${mtx}")
    for ncpus in "${NCPUS[@]}"; do
        echo "--- Running benchmarks for ${mtx_basename} with ${ncpus} CPUs ---"

        # Run gapbs
        echo "Running gapbs..."
        PAPI_OUTPUT_DIRECTORY="papi/gapbs_${ncpus}cpus_${mtx_basename}" \
        PAPI_EVENTS="${PAPI_EVENTS}" \
        OMP_NUM_THREADS=${ncpus} ./gapbs/targets/bfs_papi -f "${mtx}" -n ${N}

        # Run openmp
        echo "Running openmp..."
        PAPI_OUTPUT_DIRECTORY="papi/openmp_${ncpus}cpus_${mtx_basename}" \
        PAPI_EVENTS="${PAPI_EVENTS}" \
        OMP_NUM_THREADS=${ncpus} ./openmp/targets/bfs_papi "${mtx}" ${N} merged_csr_distances

        # Run pthreads
        echo "Running pthreads..."
        PAPI_OUTPUT_DIRECTORY="papi/pthreads_${ncpus}cpus_${mtx_basename}" \
        PAPI_EVENTS="${PAPI_EVENTS}" \
        ./pthreads/targets/bfs_${ncpus}_papi -f "${mtx}" -n ${N}
    done
done

echo "All benchmarks finished."