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
  _Atomic(Chunk **) chunks;
  _Atomic int chunks_size;
  _Atomic int top_chunk;
} ThreadChunks;

typedef struct {
  ThreadChunks **thread_chunks;
  _Atomic int *thread_chunk_counts;
} Frontier;

Frontier *frontier_create();
void frontier_destroy(Frontier *f);
Chunk *frontier_create_chunk(Frontier *f, int thread_id);
Chunk *frontier_remove_chunk(Frontier *f, int thread_id);
void chunk_push_vertex(Chunk *c, mer_t v);
mer_t chunk_pop_vertex(Chunk *c);
int frontier_get_total_chunks(Frontier *f);

#endif // FRONTIER_H
