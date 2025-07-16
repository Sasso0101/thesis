#include "frontier.h"
#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

void allocate_chunks(ThreadChunks *thread, int count) {
  if (thread->chunks_size == 0) {
    thread->chunks = (Chunk **)malloc(INITIAL_CHUNKS_PER_THREAD * sizeof(Chunk *));
  } else {
    thread->chunks = (Chunk **)realloc(thread->chunks,
                                      (thread->chunks_size + count) * sizeof(Chunk *));
  }
  for (int i = 0; i < count; i++) {
    thread->chunks[thread->chunks_size + i] = (Chunk *)malloc(sizeof(Chunk));
    thread->chunks[thread->chunks_size + i]->next_free_index = 0;
  }
  thread->chunks_size += count;
}

Frontier *frontier_create() {
  Frontier *f = (Frontier *)malloc(sizeof(Frontier));
  f->thread_chunks =
      (ThreadChunks **)malloc(sizeof(ThreadChunks *) * MAX_THREADS);
  f->thread_chunk_counts = (atomic_int *)malloc(sizeof(atomic_int) * MAX_THREADS);

  for (int i = 0; i < MAX_THREADS; i++) {
    f->thread_chunks[i] = (ThreadChunks *)malloc(sizeof(ThreadChunks));
    f->thread_chunks[i]->chunks_size = 0;
    atomic_init(&f->thread_chunks[i]->top_chunk, 0);
    allocate_chunks(f->thread_chunks[i], INITIAL_CHUNKS_PER_THREAD);
    atomic_init(&f->thread_chunk_counts[i], 0);
  }
  return f;
}

void frontier_destroy(Frontier *f) {
  for (int i = 0; i < MAX_THREADS; i++) {
    for (int j = 0; j < f->thread_chunks[i]->chunks_size; j++) {
      free(f->thread_chunks[i]->chunks[j]);
    }
    free(f->thread_chunks[i]->chunks);
    free(f->thread_chunks[i]);
  }
  free(f->thread_chunks);
  free(f->thread_chunk_counts);
  free(f);
}

Chunk *frontier_create_chunk(Frontier *f, int thread_id) {
  ThreadChunks *thread = f->thread_chunks[thread_id];
  int current_top = atomic_load(&thread->top_chunk);
  
  if (current_top >= thread->chunks_size) {
    allocate_chunks(thread, thread->chunks_size);
  }
  
  atomic_fetch_add(&f->thread_chunk_counts[thread_id], 1);
  int new_top = atomic_fetch_add(&thread->top_chunk, 1);
  return thread->chunks[new_top];
}

Chunk *frontier_remove_chunk(Frontier *f, int thread_id) {
  ThreadChunks *thread = f->thread_chunks[thread_id];
  int current_top = atomic_load(&thread->top_chunk);
  
  if (current_top == 0) {
    return NULL;
  }
  
  int new_top = current_top - 1;
  if (atomic_compare_exchange_strong(&thread->top_chunk, &current_top, new_top)) {
    atomic_fetch_sub(&f->thread_chunk_counts[thread_id], 1);
    return thread->chunks[new_top];
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
    total += atomic_load(&f->thread_chunk_counts[i]);
  }
  return total;
}