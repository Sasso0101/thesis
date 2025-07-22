#!/bin/bash

set -e

impls=(atomic main openmp)

for impl in "${impls[@]}"; do

    cd $impl
    implname=$impl
    [[ $implname == "main" ]] && implname="mutex"
    mkdir -p targets

    if [[ $impl == "openmp" ]]; then
        cmake -B build
        make -j8 -C build BFS
        mv build/BFS "targets/bfs_${implname}"
        echo "Built: targets/bfs_${implname}"
        continue
    fi

    for chunksize in 16 32 64 256 1024 4096; do
        for ncpus in 1 2 4 8 16 32 64; do
            make clean
            CHUNK_SIZE=${chunksize} MAX_THREADS=${ncpus} CC=clang CXX=clang++ make -j8 bin/bfs
            mv bin/bfs "targets/bfs_${implname}_chunksize${chunksize}_${ncpus}cpus"
            echo "Built: targets/bfs_${implname}_chunksize${chunksize}_${ncpus}cpus"
        done
    done
    cd ..
done
