#ifndef FRONTIER_H
#define FRONTIER_H

#include "config.h"
//#include <numa.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

// Remove unused function
// static __thread uint32_t cur_thread_id = 0;
// static void init_thread_id() {
//     if (cur_thread_id == 0) {
//         cur_thread_id = 1;
//     }
// }

// CNA Lock Implementation
typedef struct cna_node {
  _Atomic(uintptr_t) spin;
  _Atomic(int) socket;
  _Atomic(struct cna_node *) secTail;
  _Atomic(struct cna_node *) next;
} cna_node_t;

typedef struct {
  _Atomic(cna_node_t *) tail;
} cna_lock_t;

// Fixed NUMA node detection
static int current_numa_node() {
  int core = sched_getcpu();
 if (core < 16) return core / 8;
    else if (core < 32) return (core - 16) / 8 + 2;
    else if (core < 48) return (core - 32) / 8;
    else return (core - 48) / 8 + 2;
//  int numa_node = numa_node_of_cpu(core);
 // return numa_node;
}

#define THRESHOLD (0xffff)
#define UNLOCK_COUNT_THRESHOLD 1024

static inline uint32_t xor_random() {
  static __thread uint32_t rv = 1; // Simplified initialization
  uint32_t v = rv;
  v ^= v << 6;
  v ^= (uint32_t)(v) >> 21;
  v ^= v << 7;
  rv = v;
  return v & (UNLOCK_COUNT_THRESHOLD - 1);
}

static inline _Bool keep_lock_local() { return xor_random() & THRESHOLD; }

static inline cna_node_t *find_successor(cna_node_t *me) {
  cna_node_t *next = atomic_load_explicit(&me->next, memory_order_relaxed);
  int mySocket = atomic_load_explicit(&me->socket, memory_order_relaxed);
  if (mySocket == -1)
    mySocket = current_numa_node();
  if (next &&
      atomic_load_explicit(&next->socket, memory_order_relaxed) == mySocket) {
    return next;
  }

  cna_node_t *secHead = next;
  cna_node_t *secTail = next;
  if (!next)
    return NULL;

  cna_node_t *cur = atomic_load_explicit(&next->next, memory_order_acquire);
  while (cur) {
    if (atomic_load_explicit(&cur->socket, memory_order_relaxed) == mySocket) {
      if (atomic_load_explicit(&me->spin, memory_order_relaxed) > 1) {
        cna_node_t *_spin =
            (cna_node_t *)atomic_load_explicit(&me->spin, memory_order_relaxed);
        cna_node_t *_secTail =
            atomic_load_explicit(&_spin->secTail, memory_order_relaxed);
        atomic_store_explicit(&_secTail->next, secHead, memory_order_relaxed);
      } else {
        atomic_store_explicit(&me->spin, (uintptr_t)secHead,
                              memory_order_relaxed);
      }
      atomic_store_explicit(&secTail->next, NULL, memory_order_relaxed);
      cna_node_t *_spin =
          (cna_node_t *)atomic_load_explicit(&me->spin, memory_order_relaxed);
      atomic_store_explicit(&_spin->secTail, secTail, memory_order_relaxed);
      return cur;
    }
    secTail = cur;
    cur = atomic_load_explicit(&cur->next, memory_order_acquire);
  }
  return NULL;
}

static inline void cna_lock(cna_lock_t *lock, cna_node_t *me) {
  atomic_store_explicit(&me->next, NULL, memory_order_relaxed);
  atomic_store_explicit(&me->socket, -1, memory_order_relaxed);
  atomic_store_explicit(&me->spin, 0, memory_order_relaxed);

  cna_node_t *tail =
      atomic_exchange_explicit(&lock->tail, me, memory_order_seq_cst);

  if (!tail) {
    atomic_store_explicit(&me->spin, 1, memory_order_relaxed);
    return;
  }

  atomic_store_explicit(&me->socket, current_numa_node(), memory_order_relaxed);
  atomic_store_explicit(&tail->next, me, memory_order_release);

  while (!atomic_load_explicit(&me->spin, memory_order_acquire)) {
    __asm__ __volatile__("nop");
  }
}

static inline void cna_unlock(cna_lock_t *lock, cna_node_t *me) {
  cna_node_t *next = atomic_load_explicit(&me->next, memory_order_acquire);

  if (!next) {
    uintptr_t spin = atomic_load_explicit(&me->spin, memory_order_relaxed);
    if (spin == 1) {
      cna_node_t *expected = me;
      if (atomic_compare_exchange_strong_explicit(&lock->tail, &expected, NULL,
                                                  memory_order_seq_cst,
                                                  memory_order_relaxed)) {
        return;
      }
    } else {
      cna_node_t *secHead = (cna_node_t *)spin;
      cna_node_t *expected = me;
      if (atomic_compare_exchange_strong_explicit(
              &lock->tail, &expected,
              atomic_load_explicit(&secHead->secTail, memory_order_relaxed),
              memory_order_seq_cst, memory_order_relaxed)) {
        atomic_store_explicit(&secHead->spin, 1, memory_order_release);
        return;
      }
    }

    while (!(next = atomic_load_explicit(&me->next, memory_order_acquire))) {
      __asm__ __volatile__("nop");
    }
  }

  cna_node_t *succ = NULL;
  if (keep_lock_local() && (succ = find_successor(me))) {
    atomic_store_explicit(&succ->spin,
                          atomic_load_explicit(&me->spin, memory_order_relaxed),
                          memory_order_release);
  } else if (atomic_load_explicit(&me->spin, memory_order_relaxed) > 1) {
    succ = (cna_node_t *)atomic_load_explicit(&me->spin, memory_order_relaxed);
    cna_node_t *secTail =
        atomic_load_explicit(&succ->secTail, memory_order_relaxed);
    atomic_store_explicit(&secTail->next, next, memory_order_relaxed);
    atomic_store_explicit(&succ->spin, 1, memory_order_release);
  } else {
    atomic_store_explicit(&next->spin, 1, memory_order_release);
  }
}

// Frontier implementation
typedef mer_t ver_t;

typedef struct {
  ver_t vertices[CHUNK_SIZE];
  int next_free_index;
} Chunk;

typedef struct {
  Chunk **chunks;  // Array of pointers to chunks
  int chunks_size; // Current size of the chunks array
  int top_chunk;   // Index of the next chunk to be allocated
  cna_lock_t lock; // Changed from pthread_mutex_t to cna_lock_t
  cna_node_t node; // Added for CNA lock
} ThreadChunks;

typedef struct {
  ThreadChunks **thread_chunks;
  int *thread_chunk_counts;
} Frontier;

Frontier *frontier_create();
void frontier_destroy(Frontier *f);
Chunk *frontier_create_chunk(Frontier *f, int thread_id);
Chunk *frontier_remove_chunk(Frontier *f, int thread_id);
void chunk_push_vertex(Chunk *c, mer_t v);
mer_t chunk_pop_vertex(Chunk *c);
int frontier_get_total_chunks(Frontier *f);

#endif // FRONTIER_H
