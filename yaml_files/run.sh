#!/bin/bash

# Define the variable configurations
NCPUS=(1 2 4 8 16)
# NCPUS=(1 2 4 8 16 32)
CHUNKSIZES=(4 8 16 32 64 256 1024)
DATASETS=("datasets/GAP-road.mtx" "datasets/Geo_1438.mtx" "datasets/Hook_1498.mtx"  "datasets/PFlow_742.mtx" "datasets/asia_osm.mtx" "datasets/europe_osm.mtx" "datasets/rgg_n_2_22_s0.mtx")
N=32

# Create a directory for the logs if it doesn't exist
mkdir -p logs

# Generate a single timestamp for this script run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "Starting benchmark run. Logs will be saved in the 'logs/' directory."
echo "-------------------------------------------"


# --- Pthreads Execution ---
echo "Running Pthreads benchmarks..."
for ncpus in "${NCPUS[@]}"; do
  for dataset in "${DATASETS[@]}"; do
    for chunksize in "${CHUNKSIZES[@]}"; do
      dataset_basename=$(basename "$dataset")
      LOG_FILE="logs/pthreads ${dataset_basename} ${ncpus}cpus chunk${chunksize} ${TIMESTAMP}.log"
      
      echo "  - Pthreads: CPUs=$ncpus, Dataset=$dataset, Chunksize=$chunksize -> ${LOG_FILE}"
      # Write header to log
      echo "$ncpus, $dataset, $chunksize" > "$LOG_FILE"
      # Execute and append stdout/stderr to log
      ./pthreads/targets/bfs_chunksize${chunksize}_${ncpus}cpus -f "${dataset}" -n ${N} >> "$LOG_FILE" 2>&1
    done
  done
done
echo "Pthreads benchmarks complete."
echo "-------------------------------------------"


# --- OpenMP Execution ---
echo "Running OpenMP benchmarks..."
for ncpus in "${NCPUS[@]}"; do
  for dataset in "${DATASETS[@]}"; do
    dataset_basename=$(basename "$dataset")
    LOG_FILE="logs/openmp ${dataset_basename} ${ncpus}cpus ${TIMESTAMP}.log"

    echo "  - OpenMP: CPUs=$ncpus, Dataset=$dataset -> ${LOG_FILE}"
    # Write header to log (chunksize is 0)
    echo "$ncpus, $dataset, 0" > "$LOG_FILE"
    # Execute and append stdout/stderr to log
    OMP_NUM_THREADS=${ncpus} openmp/bin/bfs "${dataset}" ${N} merged_csr_distances >> "$LOG_FILE" 2>&1
  done
done
echo "OpenMP benchmarks complete."
echo "-------------------------------------------"


# --- GAP-BS Execution ---
echo "Running GAP-BS benchmarks..."
for ncpus in "${NCPUS[@]}"; do
  for dataset in "${DATASETS[@]}"; do
    dataset_basename=$(basename "$dataset")
    LOG_FILE="logs/gapbs ${dataset_basename} ${ncpus}cpus ${TIMESTAMP}.log"

    echo "  - GAP-BS: CPUs=$ncpus, Dataset=$dataset -> ${LOG_FILE}"
    # Write header to log (chunksize is 0)
    echo "$ncpus, $dataset, 0" > "$LOG_FILE"
    # Execute and append stdout/stderr to log
    OMP_NUM_THREADS=${ncpus} gapbs/bfs -f "${dataset}" -n ${N} >> "$LOG_FILE" 2>&1
  done
done
echo "GAP-BS benchmarks complete."
echo "-------------------------------------------"
echo "All benchmarks finished."