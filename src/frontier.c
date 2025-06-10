#include "frontier.h"
#include "config.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

Frontier *frontier_create() {
  Frontier *f = (Frontier *)malloc(sizeof(Frontier));
  f->thread_chunks =
      (ThreadChunks **)malloc(sizeof(ThreadChunks *) * MAX_THREADS);
  f->chunk_counts = (int *)calloc(MAX_THREADS, sizeof(int));

  for (int i = 0; i < MAX_THREADS; i++) {
    f->thread_chunks[i] = (ThreadChunks *)malloc(sizeof(ThreadChunks));
    f->thread_chunks[i]->chunks =
        (Chunk **)malloc(sizeof(Chunk *) * CHUNKS_PER_THREAD);
    f->thread_chunks[i]->chunks_size = CHUNKS_PER_THREAD;
    f->thread_chunks[i]->chunks[0] = (Chunk *)malloc(sizeof(Chunk));
    f->thread_chunks[i]->chunks[0]->next_free_index = 0;
    f->thread_chunks[i]->scratch_chunk = f->thread_chunks[i]->chunks[0];
    f->thread_chunks[i]->top_chunk = 0;
    f->thread_chunks[i]->initialized_count = 1;
    f->thread_chunks[i]->next_stealable_thread = (i + 1) % MAX_THREADS;
    f->chunk_counts[i] = 0;
    pthread_mutex_init(&f->thread_chunks[i]->lock, NULL);
  }
  return f;
}

void destroy_frontier(Frontier *f) {
  for (int i = 0; i < MAX_THREADS; i++) {
    for (int j = 0; j < f->thread_chunks[i]->initialized_count; j++) {
      free(f->thread_chunks[i]->chunks[j]);
    }
    free(f->thread_chunks[i]->chunks);
    pthread_mutex_destroy(&f->thread_chunks[i]->lock);
  }
  free(f->thread_chunks);
  free(f->chunk_counts);
  free(f);
}

/**
 * Creates an empty scratch chunk by pointing it to the top chunk in the chunk
 * array. If the chunk array is full, it enlarges it by a factor of two.
 */
void thread_new_scratch_chunk(ThreadChunks *t, int *chunk_counter) {
  pthread_mutex_lock(&t->lock);
  t->top_chunk++;
  // Check if we need to resize the chunks array
  if (t->top_chunk >= t->chunks_size) {
    t->chunks_size *= 2;
    t->chunks = (Chunk **)realloc(t->chunks, t->chunks_size * sizeof(Chunk *));
  }
  // Check if we need to allocate a new chunk
  if (t->top_chunk >= t->initialized_count) {
    t->chunks[t->top_chunk] = (Chunk *)malloc(sizeof(Chunk));
    t->chunks[t->top_chunk]->next_free_index = 0;
    t->initialized_count++;
  }
  (*chunk_counter)++;
  pthread_mutex_unlock(&t->lock);
  t->scratch_chunk = t->chunks[t->top_chunk];
}

/**
 * Removes the top chunk from the chunk array and returns a pointer to it. If
 * the chunk array is empty, returns NULL.
 */
Chunk *thread_remove_chunk(ThreadChunks *t, int *chunk_counter) {
  pthread_mutex_lock(&t->lock);
  Chunk *c;
  // Check if thread has chunks
  if (t->top_chunk > 0) {
    t->top_chunk--;
    c = t->chunks[t->top_chunk];
    (*chunk_counter)--;
  } else {
    c = NULL;
  }
  pthread_mutex_unlock(&t->lock);
  return c;
}

/**
 * Adds a vertex to the next available position in the chunk.
 * Asserts that the chunk is valid and not full.
 *
 * @note This function is not thread-safe. Caller must ensure exclusive access.
 */
void chunk_push_vertex(Chunk *c, ver_t v) {
  assert(c != NULL && "Trying to insert in NULL chunk!");
  assert(c->next_free_index < CHUNK_SIZE && "Trying to insert in full chunk!");
  c->vertices[c->next_free_index] = v;
  c->next_free_index++;
}

/**
 * Removes and returns the vertex at the top of the chunk (LIFO order).
 *
 * @note This function is not thread-safe. Caller must ensure exclusive access.
 */
ver_t chunk_pop_vertex(Chunk *c) {
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
    total += f->chunk_counts[i] +
             (f->thread_chunks[i]->scratch_chunk->next_free_index > 0 ? 1 : 0);
  }
  return total;
}

void frontier_push_vertex(Frontier *f, int thread_id, ver_t v) {
  ThreadChunks *t = f->thread_chunks[thread_id];
  if (t->scratch_chunk->next_free_index == CHUNK_SIZE) {
    thread_new_scratch_chunk(t, &f->chunk_counts[thread_id]);
  }
  chunk_push_vertex(t->scratch_chunk, v);
}

ver_t frontier_pop_vertex(Frontier *f, int thread_id) {
  ThreadChunks *t = f->thread_chunks[thread_id];
  // If scratch_chunk is empty, take a chunk from the top of thread chunk array
  if (t->scratch_chunk->next_free_index == 0) {
    Chunk *c = NULL;
    if ((c = thread_remove_chunk(t, &f->chunk_counts[thread_id])) != NULL) {
      t->scratch_chunk = c;
    } else {
      // If thread chunk array is empty, steal a chunk from another thread by
      // pointing scratch_chunk to it
      if (t->scratch_chunk == NULL) {
        int *next_stealable_thread =
            &f->thread_chunks[thread_id]->next_stealable_thread;
        while (f->chunk_counts[*next_stealable_thread] == 0 &&
               *next_stealable_thread != thread_id) {
          *next_stealable_thread = (*next_stealable_thread + 1) % MAX_THREADS;
        }
        if (*next_stealable_thread == thread_id) {
          return VERT_MAX; // No threads to steal from
        } else {
          t->scratch_chunk =
              thread_remove_chunk(f->thread_chunks[*next_stealable_thread],
                                  &f->chunk_counts[*next_stealable_thread]);
        }
      }
    }
  }
  return chunk_pop_vertex(t->scratch_chunk);
}
