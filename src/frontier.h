#ifndef FRONTIER_H
#define FRONTIER_H

/**
 * @brief Thread-local vertex chunk pool for parallel graph traversal.
 *
 * The Frontier data structure manages per-thread pools of reusable vertex
 * chunks for use in parallel graph algorithms. Each thread maintains its own
 * chunk pool to minimize contention and improve cache locality.
 *
 * ## Design Overview
 * - The Frontier is composed of MAX_THREADS thread-local chunk pools.
 * - Each pool (`ChunkPool`) contains a dynamically resizable array of
 *   pointers to `VertexChunk` blocks.
 * - A `VertexChunk` holds a fixed-size array of vertices (CHUNK_SIZE) and acts
 *   as a LIFO (stack-style) buffer.
 * - Threads acquire and release chunks from their own pool in a lock-protected
 * manner.
 * - Chunks are not freed when released — they are reused, minimizing dynamic
 * allocations.
 *
 * ## Concurrency Model
 * - Chunk acquisition and release are thread-safe for individual threads (each
 * thread locks only its own pool).
 * - No synchronization is required for accessing the internal content of a
 * `VertexChunk` if it is used exclusively by one thread.
 * - Cross-thread stealing is supported via inspection functions.
 */

#include "config.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

typedef mer_t ver_t;

typedef struct {
  ver_t vertices[CHUNK_SIZE];
  int next_free_index;
} Chunk;

typedef struct {
  Chunk **chunks;
  int chunks_size;
  int initialized_count;
  int top_chunk;
  int next_stealable_thread;
  Chunk *scratch_chunk; // Scratch chunk used when adding or removing chunks
  pthread_mutex_t lock;
} ThreadChunks;

typedef struct {
  ThreadChunks **thread_chunks;
  int *chunk_counts;
} Frontier;

/**
 * Creates and initializes a new Frontier structure. Allocates memory for
 * per-thread vertex chunk pools and sets up mutexes for thread-safe chunk
 * acquisition and release.
 */
Frontier *frontier_create();

/**
 * Destroys a Frontier structure and deallocates all associated resources.
 */
void destroy_frontier(Frontier *f);

/**
 * Calculates the total number of in-use chunks across all threads.
 *
 * @note This function is not thread-safe.
 */
int frontier_get_total_chunks(Frontier *f);

/*
 * Pushes a vertex in the ThreadChunks scratch chunk. If the chunk is full, it
 * pushes it to the chunks array.
 */
void frontier_push_vertex(Frontier *f, int thread_id, ver_t v);

/*
 * Pops a vertex from the ThreadChunks scratch chunk. If it is empty, it pops a
 * chunk from the chunks array. If the chunk array is empty, it attempts to
 * steal chunks from other threads. If there is no chunk to steal available, it
 * returns VERT_MAX.
 */
ver_t frontier_pop_vertex(Frontier *f, int thread_id);

#endif // FRONTIER_H
