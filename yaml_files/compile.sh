# Log file with timestamp
LOG_FILE="compile_log_$(date +%Y%m%d_%H%M%S).log"

# Redirect stdout and stderr to a log file and the console
exec > >(tee -a "${LOG_FILE}") 2>&1

echo "Starting compilation process..."
echo "Logging to ${LOG_FILE}"

# Define possible values
chunksizes=(4 8 15 32 64 256 1024 4096)
ncpus_list=(1 2 4 8 16 32)

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

# Compile openmp
echo "--- Compiling openmp target ---"
(
    cd openmp && \
    make clean && \
    make -j24 bin/bfs && \
    echo "Built: openmp/bin/bfs"
) || echo "ERROR: Failed to build openmp target"
echo "--- Finished openmp target ---"
echo ""

# Compile gapbs
echo "--- Compiling gapbs target ---"
(
    cd gapbs && \
    make clean && \
    make -j24 bfs && \
    echo "Built: gapbs/bfs"
) || echo "ERROR: Failed to build gapbs target"
echo "--- Finished gapbs target ---"
echo ""

echo "Compilation script finished."

