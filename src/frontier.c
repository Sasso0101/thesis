#include "frontier.h"
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void allocate_chunks(ThreadChunks *thread, int count) {
  thread->chunks = (Chunk **)realloc(
      thread->chunks, (thread->chunks_size + count) * sizeof(Chunk *));
  for (int i = 0; i < count; i++) {
    thread->chunks[thread->top_chunk + i] = (Chunk *)malloc(sizeof(Chunk));
    thread->chunks[thread->top_chunk + i]->next_free_index = 0;
  }
  thread->chunks_size += count;
}

Frontier *frontier_create() {
  Frontier *f = (Frontier *)malloc(sizeof(Frontier));
  f->thread_chunks =
      (ThreadChunks **)malloc(sizeof(ThreadChunks *) * MAX_THREADS);
  f->thread_chunk_counts =
      (_Atomic(int) *)malloc(sizeof(_Atomic(int)) * MAX_THREADS);

  for (int i = 0; i < MAX_THREADS; i++) {
    f->thread_chunks[i] = (ThreadChunks *)malloc(sizeof(ThreadChunks));
    f->thread_chunks[i]->chunks_size = 0;
    allocate_chunks(f->thread_chunks[i], INITIAL_CHUNKS_PER_THREAD);
    f->thread_chunks[i]->top_chunk = 0;
    f->thread_chunk_counts[i] = 0;
  }
  return f;
}

void frontier_destroy(Frontier *f) {
  for (int i = 0; i < MAX_THREADS; i++) {
    for (int j = 0; j < f->thread_chunks[i]->chunks_size; j++) {
      free(f->thread_chunks[i]->chunks[j]);
    }
    free(f->thread_chunks[i]->chunks);
    // No need to destroy CNA lock as it has no resources to free
  }
  free(f->thread_chunks);
  free(f);
}

Chunk *frontier_create_chunk(Frontier *f, int thread_id) {
  ThreadChunks *thread = f->thread_chunks[thread_id];
  // Check if the thread's chunks array is full
  if (thread->top_chunk >= thread->chunks_size) {
    // Double the size of the chunks array
    allocate_chunks(thread, thread->chunks_size);
  }
  f->thread_chunk_counts[thread_id]++;
  return thread->chunks[thread->top_chunk++];
}

Chunk *frontier_remove_chunk(Frontier *f, int thread_id) {
  ThreadChunks *thread = f->thread_chunks[thread_id];
  int old_top = atomic_load(&thread->top_chunk);
  if (old_top > 0) {
    if (atomic_compare_exchange_weak(&thread->top_chunk, &old_top,
                                     old_top - 1)) {
      Chunk *chunk = thread->chunks[old_top - 1];
      atomic_fetch_sub(&thread->top_chunk, 1);
      f->thread_chunk_counts[thread_id]--;
      return chunk;
    }
  }
  return NULL;
}

void chunk_push_vertex(Chunk *c, mer_t v) {
  assert(c != NULL && "Trying to insert in NULL chunk!");
  assert(c->next_free_index < CHUNK_SIZE && "Trying to insert in full chunk!");
  c->vertices[c->next_free_index] = v;
  c->next_free_index++;
}

mer_t chunk_pop_vertex(Chunk *c) {
  if (c->next_free_index > 0) {
    c->next_free_index--;
    return c->vertices[c->next_free_index];
  } else {
    return VERT_MAX;
  }
}

int frontier_get_total_chunks(Frontier *f) {
  int total = 0;
  for (int i = 0; i < MAX_THREADS; i++) {
    total += f->thread_chunk_counts[i];
  }
  return total;
}