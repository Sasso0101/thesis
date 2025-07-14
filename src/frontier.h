#ifndef FRONTIER_H
#define FRONTIER_H

#include "config.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef mer_t ver_t;

typedef struct {
    ver_t vertices[CHUNK_SIZE];
    _Atomic int next_free_index;
} Chunk;

typedef struct {
    _Atomic(Chunk**) chunks;
    _Atomic int chunks_size;
    _Atomic int initialized_count;
    _Atomic int top_chunk;
    _Atomic int next_stealable_thread;
    _Atomic(Chunk*) scratch_chunk;
} ThreadChunks;

typedef struct {
    ThreadChunks** thread_chunks;
    _Atomic int* chunk_counts;
} Frontier;

Frontier* frontier_create();
void destroy_frontier(Frontier* f);
int frontier_get_total_chunks(Frontier* f);
void frontier_push_vertex(Frontier* f, int thread_id, ver_t v);
ver_t frontier_pop_vertex(Frontier* f, int thread_id);

#endif // FRONTIER_H
