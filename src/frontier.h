#ifndef FRONTIER_H
#define FRONTIER_H

#include "config.h"
#include <stdbool.h>
#include <stddef.h>  // Added for NULL
#include <stdint.h>

typedef mer_t ver_t;

// MCS Lock structures
typedef struct mcs_node {
    struct mcs_node *next;
    int locked;
} mcs_node;

typedef struct {
    mcs_node *tail;
} mcs_lock;

// Initialize an MCS lock
static inline void mcs_lock_init(mcs_lock *lock) {
    lock->tail = NULL;
}

// MCS lock acquire
static inline void mcs_lock_acquire(mcs_lock *lock, mcs_node *node) {
    node->next = NULL;
    node->locked = 1;
    
    mcs_node *prev = __atomic_exchange_n(&lock->tail, node, __ATOMIC_RELEASE);
    if (prev != NULL) {
        prev->next = node;
        while (node->locked) {
      __asm__ __volatile__("nop");
        }
    }
}

// MCS lock release
static inline void mcs_lock_release(mcs_lock *lock, mcs_node *node) {
    if (node->next == NULL) {
        mcs_node *expected = node;
        if (__atomic_compare_exchange_n(&lock->tail, &expected, NULL, 
                                     0, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            return;
        }
        while (node->next == NULL) {
      __asm__ __volatile__("nop");
        }
    }
    node->next->locked = 0;
}

// Chunk structure
typedef struct {
    ver_t vertices[CHUNK_SIZE];
    int next_free_index;
} Chunk;

// Thread-specific chunk pool
typedef struct {
    Chunk **chunks;
    int chunks_size;
    int initialized_count;
    int top_chunk;
    int next_stealable_thread;
    Chunk *scratch_chunk;
    mcs_lock lock;
    mcs_node node;
} ThreadChunks;

// Frontier structure
typedef struct {
    ThreadChunks **thread_chunks;
    int *chunk_counts;
} Frontier;

// Frontier API
Frontier *frontier_create();
void destroy_frontier(Frontier *f);
int frontier_get_total_chunks(Frontier *f);
void frontier_push_vertex(Frontier *f, int thread_id, ver_t v);
ver_t frontier_pop_vertex(Frontier *f, int thread_id);

#endif // FRONTIER_H
