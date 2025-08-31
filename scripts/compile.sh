#!/bin/bash

# Log file with timestamp
LOG_FILE="compile_log_$(date +%Y%m%d_%H%M%S).log"

# Redirect stdout and stderr to a log file and the console
exec > >(tee -a "${LOG_FILE}") 2>&1

echo "Starting compilation process..."
echo "Logging to ${LOG_FILE}"

# Define possible values
chunksizes=(4 8 15 32 64 256 1024 4096)
ncpus_list=(1 2 4 8 16 32)
PTHREADS_NCPUS_LIST=(2 4 8 16 32)

# Compile pthreads with different configurations
echo "--- Compiling pthreads targets ---"
for chunksize in "${chunksizes[@]}"; do
    for ncpus in "${ncpus_list[@]}"; do
        echo "Building pthreads with CHUNK_SIZE=${chunksize} NCPUS=${ncpus}"
        (
            cd pthreads && \
            mkdir -p targets && \
            make clean && \
            CHUNK_SIZE=${chunksize} MAX_THREADS=${ncpus} make -j24 bin/bfs && \
            mv bin/bfs "targets/bfs_chunksize${chunksize}_${ncpus}cpus" && \
            echo "Built: pthreads/targets/bfs_chunksize${chunksize}_${ncpus}cpus"
        ) || echo "ERROR: Failed to build pthreads with CHUNK_SIZE=${chunksize} NCPUS=${ncpus}"
        echo "" 
    done
done
echo "--- Finished pthreads targets ---"
echo ""

# Compile pthreads likwid (single-core version)
echo "Compiling pthreads likwid for 1 core (chunksize 1024)..."
(
  cd pthreads
  mkdir -p targets
  make clean
  CHUNK_SIZE=1024 MAX_THREADS=1 make USE_LIKWID=1 -j16 bin/bfs
  mv bin/bfs targets/bfs_1_likwid
)
echo "pthreads likwid (1 core) compiled successfully."

# Compile pthreads likwid (multi-core versions)
for n in "${PTHREADS_NCPUS_LIST[@]}"; do
    echo "Compiling pthreads likwid for ${n} cores (chunksize 64)..."
    (
        cd pthreads
        make clean
        CHUNK_SIZE=64 MAX_THREADS=${n} make USE_LIKWID=1 -j16 bin/bfs
        mv bin/bfs "targets/bfs_${n}_likwid"
    )
    echo "pthreads likwid (${n} cores) compiled successfully."
done



# Compile openmp
echo "--- Compiling openmp target ---"
(
    cd openmp && \
    mkdir -p targets && \
    make clean && \
    make -j24 bin/bfs && \
    mv bin/bfs targets/bfs && \
    echo "Built: openmp/bin/bfs"
) || echo "ERROR: Failed to build openmp target"
echo "--- Finished openmp target ---"
echo ""

echo "Compiling openmp likwid..."
(
  cd openmp
  mkdir -p targets
  make clean
  make USE_LIKWID=1 -j16 bin/bfs
  mv bin/bfs targets/bfs_likwid
)
echo "openmp likwid compiled successfully."



# Compile gapbs
echo "--- Compiling gapbs target ---"
(
    cd gapbs && \
    mkdir -p targets && \
    make clean && \
    make -j24 bfs && \
    mv bfs targets/bfs && \
    echo "Built: gapbs/bfs"
) || echo "ERROR: Failed to build gapbs target"
echo "--- Finished gapbs target ---"
echo ""

echo "Compiling gapbs likwid..."
(
  cd gapbs
  mkdir -p targets
  make clean
  make USE_LIKWID=1 -j16 bfs
  mv bfs targets/bfs_likwid
)
echo "gapbs likwid compiled successfully."

echo "Compilation script finished."