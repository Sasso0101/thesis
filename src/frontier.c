#include "frontier.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

Frontier *frontier_create() {
    Frontier *f = (Frontier *)malloc(sizeof(Frontier));
    f->thread_chunks = (ThreadChunks **)malloc(sizeof(ThreadChunks *) * MAX_THREADS);
    f->chunk_counts = (int *)calloc(MAX_THREADS, sizeof(int));

    for (int i = 0; i < MAX_THREADS; i++) {
        f->thread_chunks[i] = (ThreadChunks *)malloc(sizeof(ThreadChunks));
        f->thread_chunks[i]->chunks = (Chunk **)malloc(sizeof(Chunk *) * CHUNKS_PER_THREAD);
        f->thread_chunks[i]->chunks_size = CHUNKS_PER_THREAD;
        f->thread_chunks[i]->chunks[0] = (Chunk *)malloc(sizeof(Chunk));
        f->thread_chunks[i]->chunks[0]->next_free_index = 0;
        f->thread_chunks[i]->scratch_chunk = f->thread_chunks[i]->chunks[0];
        f->thread_chunks[i]->top_chunk = 0;
        f->thread_chunks[i]->initialized_count = 1;
        f->thread_chunks[i]->next_stealable_thread = (i + 1) % MAX_THREADS;
        f->chunk_counts[i] = 0;
        f->thread_chunks[i]->lock.tail = NULL;
    }
    return f;
}

void destroy_frontier(Frontier *f) {
    for (int i = 0; i < MAX_THREADS; i++) {
        for (int j = 0; j < f->thread_chunks[i]->initialized_count; j++) {
            free(f->thread_chunks[i]->chunks[j]);
        }
        free(f->thread_chunks[i]->chunks);
        free(f->thread_chunks[i]);
    }
    free(f->thread_chunks);
    free(f->chunk_counts);
    free(f);
}

void thread_new_scratch_chunk(ThreadChunks *t, int *chunk_counter) {
    mcs_lock_acquire(&t->lock, &t->node);
    t->top_chunk++;
    if (t->top_chunk >= t->chunks_size) {
        t->chunks_size *= 2;
        t->chunks = (Chunk **)realloc(t->chunks, t->chunks_size * sizeof(Chunk *));
    }
    if (t->top_chunk >= t->initialized_count) {
        t->chunks[t->top_chunk] = (Chunk *)malloc(sizeof(Chunk));
        t->chunks[t->top_chunk]->next_free_index = 0;
        t->initialized_count++;
    }
    (*chunk_counter)++;
    mcs_lock_release(&t->lock, &t->node);
    t->scratch_chunk = t->chunks[t->top_chunk];
}

Chunk *thread_remove_chunk(ThreadChunks *t, int *chunk_counter) {
    mcs_lock_acquire(&t->lock, &t->node);
    Chunk *c = NULL;
    if (t->top_chunk > 0) {
        t->top_chunk--;
        c = t->chunks[t->top_chunk];
        (*chunk_counter)--;
    }
    mcs_lock_release(&t->lock, &t->node);
    return c;
}

void chunk_push_vertex(Chunk *c, ver_t v) {
    assert(c != NULL && "Trying to insert in NULL chunk!");
    assert(c->next_free_index < CHUNK_SIZE && "Trying to insert in full chunk!");
    c->vertices[c->next_free_index] = v;
    c->next_free_index++;
}

ver_t chunk_pop_vertex(Chunk *c) {
    if (c->next_free_index > 0) {
        c->next_free_index--;
        return c->vertices[c->next_free_index];
    }
    return VERT_MAX;
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
    if (t->scratch_chunk->next_free_index == 0) {
        Chunk *c = thread_remove_chunk(t, &f->chunk_counts[thread_id]);
        if (c != NULL) {
            t->scratch_chunk = c;
        } else {
            if (t->scratch_chunk == NULL) {
                int *next_stealable_thread = &t->next_stealable_thread;
                while (f->chunk_counts[*next_stealable_thread] == 0 &&
                      *next_stealable_thread != thread_id) {
                    *next_stealable_thread = (*next_stealable_thread + 1) % MAX_THREADS;
                }
                if (*next_stealable_thread == thread_id) {
                    return VERT_MAX;
                } else {
                    t->scratch_chunk = thread_remove_chunk(
                        f->thread_chunks[*next_stealable_thread],
                        &f->chunk_counts[*next_stealable_thread]);
                }
            }
        }
    }
    return chunk_pop_vertex(t->scratch_chunk);
}