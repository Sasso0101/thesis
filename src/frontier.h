#ifndef FRONTIER_H
#define FRONTIER_H

#include "config.h"
#include <stdatomic.h>
#include <stdbool.h>

typedef mer_t ver_t;

typedef struct {
  ver_t vertices[CHUNK_SIZE];
  int next_free_index;
} Chunk;

typedef struct {
  Chunk **chunks;        // Array of pointers to chunks
  int chunks_size;       // Current size of the chunks array
  atomic_int top_chunk;  // Atomic index of the next chunk to be allocated
} ThreadChunks;

typedef struct {
  ThreadChunks **thread_chunks;
  atomic_int *thread_chunk_counts;
} Frontier;

Frontier *frontier_create();
void frontier_destroy(Frontier *f);
Chunk *frontier_create_chunk(Frontier *f, int thread_id);
Chunk *frontier_remove_chunk(Frontier *f, int thread_id);
void chunk_push_vertex(Chunk *c, mer_t v);
mer_t chunk_pop_vertex(Chunk *c);
int frontier_get_total_chunks(Frontier *f);

#endif // FRONTIER_H