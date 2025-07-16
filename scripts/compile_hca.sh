#!/bin/bash

set -e

module load llvm/cross/EPI-development cmake/3.23.2

impls=(atomic cnalock main mcslock openmp)

for impl in "${impls[@]}"; do
    cd $impl
    implname=$impl
    [[ $implname == "main" ]] && implname="mutex"
    for ncpus in 1 2 4 8 16 32 64; do
        for chunksize in 16 32 64 256 1024 4096; do
            make clean
            CHUNK_SIZE=${chunksize} MAX_THREADS=${ncpus} CC=clang CXX=clang++ make -j16 bin/bfs
            mkdir -p targets
            mv bin/bfs "targets/bfs_${implname}_chunksize${chunksize}_${ncpus}cpus"
            echo "Built: targets/bfs_${implname}_chunksize${chunksize}_${ncpus}cpus"
        done
    done
    cd ..
done
