#include "frontier.h"
#include "config.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

Frontier* frontier_create() {
    Frontier* f = malloc(sizeof(Frontier));
    f->thread_chunks = malloc(sizeof(ThreadChunks*) * MAX_THREADS);
    f->chunk_counts = malloc(sizeof(int) * MAX_THREADS);

    for (int i = 0; i < MAX_THREADS; i++) {
        f->thread_chunks[i] = malloc(sizeof(ThreadChunks));
        ThreadChunks* tc = f->thread_chunks[i];
        
        Chunk** chunks = malloc(sizeof(Chunk*) * CHUNKS_PER_THREAD);
        chunks[0] = malloc(sizeof(Chunk));
        chunks[0]->next_free_index = 0;
        
        atomic_store(&tc->chunks, chunks);
        atomic_store(&tc->chunks_size, CHUNKS_PER_THREAD);
        atomic_store(&tc->scratch_chunk, chunks[0]);
        atomic_store(&tc->top_chunk, 0);
        atomic_store(&tc->initialized_count, 1);
        atomic_store(&tc->next_stealable_thread, (i + 1) % MAX_THREADS);
        atomic_store(&f->chunk_counts[i], 0);
    }
    return f;
}

void destroy_frontier(Frontier* f) {
    for (int i = 0; i < MAX_THREADS; i++) {
        ThreadChunks* tc = f->thread_chunks[i];
        Chunk** chunks = atomic_load(&tc->chunks);
        
        for (int j = 0; j < atomic_load(&tc->initialized_count); j++) {
            free(chunks[j]);
        }
        free(chunks);
        free(tc);
    }
    free(f->thread_chunks);
    free(f->chunk_counts);
    free(f);
}

void thread_new_scratch_chunk(ThreadChunks* t, _Atomic int* chunk_counter) {
    int old_top = atomic_fetch_add(&t->top_chunk, 1);
    int chunks_size = atomic_load(&t->chunks_size);
    
    if (old_top + 1 >= chunks_size) {
        int new_size = chunks_size * 2;
        Chunk** new_chunks = malloc(sizeof(Chunk*) * new_size);
        Chunk** old_chunks = atomic_load(&t->chunks);
        
        for (int i = 0; i < chunks_size; i++) {
            new_chunks[i] = old_chunks[i];
        }
        
        if (atomic_compare_exchange_strong(&t->chunks, &old_chunks, new_chunks)) {
            atomic_store(&t->chunks_size, new_size);
            free(old_chunks);
        } else {
            free(new_chunks);
        }
    }
    
    if (old_top + 1 >= atomic_load(&t->initialized_count)) {
        Chunk* new_chunk = malloc(sizeof(Chunk));
        new_chunk->next_free_index = 0;
        
        Chunk** chunks = atomic_load(&t->chunks);
        chunks[old_top + 1] = new_chunk;
        atomic_fetch_add(&t->initialized_count, 1);
    }
    
    atomic_fetch_add(chunk_counter, 1);
    atomic_store(&t->scratch_chunk, atomic_load(&t->chunks)[old_top + 1]);
}

Chunk* thread_remove_chunk(ThreadChunks* t, _Atomic int* chunk_counter) {
    int old_top = atomic_load(&t->top_chunk);
    while (old_top > 0) {
        if (atomic_compare_exchange_weak(&t->top_chunk, &old_top, old_top - 1)) {
            Chunk* c = atomic_load(&t->chunks)[old_top - 1];
            atomic_fetch_sub(chunk_counter, 1);
            return c;
        }
    }
    return NULL;
}

void chunk_push_vertex(Chunk* c, ver_t v) {
    assert(c != NULL && "Trying to insert in NULL chunk!");
    int idx = atomic_fetch_add(&c->next_free_index, 1);
    assert(idx < CHUNK_SIZE && "Trying to insert in full chunk!");
    c->vertices[idx] = v;
}

ver_t chunk_pop_vertex(Chunk* c) {
    int old_idx = atomic_load(&c->next_free_index);
    while (old_idx > 0) {
        if (atomic_compare_exchange_weak(&c->next_free_index, &old_idx, old_idx - 1)) {
            return c->vertices[old_idx - 1];
        }
    }
    return VERT_MAX;
}

int frontier_get_total_chunks(Frontier* f) {
    int total = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        ThreadChunks* tc = f->thread_chunks[i];
        total += atomic_load(&f->chunk_counts[i]) +
                (atomic_load(&atomic_load(&tc->scratch_chunk)->next_free_index) > 0 ? 1 : 0);
    }
    return total;
}

void frontier_push_vertex(Frontier* f, int thread_id, ver_t v) {
    ThreadChunks* t = f->thread_chunks[thread_id];
    Chunk* scratch = atomic_load(&t->scratch_chunk);
    
    if (atomic_load(&scratch->next_free_index) == CHUNK_SIZE) {
        thread_new_scratch_chunk(t, &f->chunk_counts[thread_id]);
        scratch = atomic_load(&t->scratch_chunk);
    }
    chunk_push_vertex(scratch, v);
}

ver_t frontier_pop_vertex(Frontier* f, int thread_id) {
    ThreadChunks* t = f->thread_chunks[thread_id];
    Chunk* scratch = atomic_load(&t->scratch_chunk);
    
    if (atomic_load(&scratch->next_free_index) == 0) {
        Chunk* c = thread_remove_chunk(t, &f->chunk_counts[thread_id]);
        if (c != NULL) {
            atomic_store(&t->scratch_chunk, c);
            scratch = c;
        } else {
            if (scratch == NULL) {
                int next_thread = atomic_load(&t->next_stealable_thread);
                while (atomic_load(&f->chunk_counts[next_thread]) == 0 &&
                       next_thread != thread_id) {
                    next_thread = (next_thread + 1) % MAX_THREADS;
                }
                
                if (next_thread == thread_id) {
                    return VERT_MAX;
                } else {
                    atomic_store(&t->scratch_chunk, 
                        thread_remove_chunk(f->thread_chunks[next_thread],
                                          &f->chunk_counts[next_thread]));
                    scratch = atomic_load(&t->scratch_chunk);
                }
            }
        }
    }
    return chunk_pop_vertex(scratch);
}
