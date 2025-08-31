#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Configuration ---
# Variables from jobs_papi.yaml
NCPUS_LIST=(1 2 4 8 16 32)
MTX_FILE="datasets/large_diameter/matrices_list.txt"
NUM_RUNS=32
PAPI_EVENTS="PAPI_L3_TCA,PAPI_L3_TCM"

# Variables from compile_papi.yaml
PTHREADS_CHUNKSIZE_MULTI_CORE=64
PTHREADS_CHUNKSIZE_SINGLE_CORE=1024
PTHREADS_NCPUS_LIST=(2 4 8 16 32)

# --- Environment Setup ---
echo "Setting up environment..."
export PAPI_DIR=/home/salvatore.andaloro/papi/src/install
export PATH=/home/salvatore.andaloro/papi/src/install/bin:$PATH
export LD_LIBRARY_PATH=/home/salvatore.andaloro/papi/src/install/lib:$LD_LIBRARY_PATH

# --- Compilation Phase ---
echo "Starting compilation for PAPI..."

# Compile gapbs
echo "Compiling gapbs..."
(
  cd gapbs
  mkdir -p targets
  make clean
  make USE_PAPI=1 -j16 bfs
  mv bfs targets/bfs_papi
)
echo "gapbs compiled successfully."

# Compile openmp
echo "Compiling openmp..."
(
  cd openmp
  mkdir -p targets
  make clean
  make USE_PAPI=1 -j16 bin/bfs
  mv bin/bfs targets/bfs_papi
)
echo "openmp compiled successfully."

# Compile pthreads (single-core version)
echo "Compiling pthreads for 1 core (chunksize ${PTHREADS_CHUNKSIZE_SINGLE_CORE})..."
(
  cd pthreads
  mkdir -p targets
  make clean
  CHUNK_SIZE=${PTHREADS_CHUNKSIZE_SINGLE_CORE} MAX_THREADS=1 make USE_PAPI=1 -j16 bin/bfs
  mv bin/bfs targets/bfs_1_papi
)
echo "pthreads (1 core) compiled successfully."

# Compile pthreads (multi-core versions)
for n in "${PTHREADS_NCPUS_LIST[@]}"; do
    echo "Compiling pthreads for ${n} cores (chunksize ${PTHREADS_CHUNKSIZE_MULTI_CORE})..."
    (
        cd pthreads
        make clean
        CHUNK_SIZE=${PTHREADS_CHUNKSIZE_MULTI_CORE} MAX_THREADS=${n} make USE_PAPI=1 -j16 bin/bfs
        mv bin/bfs "targets/bfs_${n}_papi"
    )
    echo "pthreads (${n} cores) compiled successfully."
done

echo "All compilations finished."

# --- Execution Phase ---
echo "Starting execution with PAPI..."
mkdir -p papi

# Read matrix files into an array
# mapfile -t mtx_files < "$MTX_FILE"
mtx_files=("datasets/large_diameter/europe_osm.mtx")

for mtx in "${mtx_files[@]}"; do
    mtx_basename=$(basename "${mtx}")
    for ncpus in "${NCPUS_LIST[@]}"; do
        echo "--- Running benchmarks for ${mtx_basename} with ${ncpus} CPUs ---"

        # Run gapbs
        echo "Running gapbs..."
        PAPI_OUTPUT_DIRECTORY="papi/gapbs_${ncpus}cpus_${mtx_basename}" \
        PAPI_EVENTS="${PAPI_EVENTS}" \
        OMP_NUM_THREADS=${ncpus} ./gapbs/targets/bfs_papi -f "${mtx}" -n ${NUM_RUNS}

        # Run openmp
        echo "Running openmp..."
        PAPI_OUTPUT_DIRECTORY="papi/openmp_${ncpus}cpus_${mtx_basename}" \
        PAPI_EVENTS="${PAPI_EVENTS}" \
        OMP_NUM_THREADS=${ncpus} ./openmp/targets/bfs_papi "${mtx}" ${NUM_RUNS} merged_csr_distances

        # Run pthreads
        echo "Running pthreads..."
        PAPI_OUTPUT_DIRECTORY="papi/pthreads_${ncpus}cpus_${mtx_basename}" \
        PAPI_EVENTS="${PAPI_EVENTS}" \
        ./pthreads/targets/bfs_${ncpus}_papi -f "${mtx}" -n ${NUM_RUNS}
    done
done

echo "All benchmarks finished."