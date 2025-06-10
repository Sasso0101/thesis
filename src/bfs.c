#define _GNU_SOURCE
#include "command_line.h"
#include "config.h"
#include "debug_utils.h"
#include "frontier.h"
#include "graph.h"
#include "merged_csr.h"
#include "mt19937-64.h"
#include "thread_pool.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

MergedCSR *merged_csr;
Frontier *f1, *f2;
uint32_t *distances;

atomic_int active_threads;

volatile uint32_t exploration_done;
volatile int distance;

void top_down(MergedCSR *merged_csr, Frontier *current_frontier,
              Frontier *next_frontier, int distance, int thread_id) {
  mer_t v;
  while ((v = frontier_pop_vertex(current_frontier, thread_id)) != VERT_MAX) {
    mer_t end = START_MERGED_INDICES(merged_csr, v) + DEGREE(merged_csr, v);
    for (mer_t i = START_MERGED_INDICES(merged_csr, v); i < end; i++) {
      mer_t neighbor = merged_csr->merged[i];
      if (DISTANCE(merged_csr, neighbor) == UINT32_MAX) {
        DISTANCE(merged_csr, neighbor) = distance;
        if (DEGREE(merged_csr, neighbor) != 1) {
          frontier_push_vertex(next_frontier, thread_id, neighbor);
        }
      }
    }
  }
}

void finalize_distances(MergedCSR *merged_csr, int thread_id) {
  // Write distances from mergedCSR to distances array
  mer_t chunk_size = merged_csr->num_vertices / MAX_THREADS;
  mer_t start = thread_id * chunk_size;
  mer_t end = (thread_id == MAX_THREADS - 1) ? merged_csr->num_vertices
                                             : (thread_id + 1) * chunk_size;
  for (mer_t i = start; i < end; i++) {
    distances[i] = DISTANCE(merged_csr, merged_csr->row_ptr[i]);
    DISTANCE(merged_csr, merged_csr->row_ptr[i]) = UINT32_MAX;
  }
}

void *thread_main(void *arg) {
  int thread_id = *(int *)arg;
  // printf("Thread %d: got work\n", thread_id);

  while (!exploration_done) {
    int old = distance;
    top_down(merged_csr, f1, f2, distance, thread_id);
    if (atomic_fetch_sub(&active_threads, 1) == 1) {
      // Swap frontiers
      active_threads = MAX_THREADS;
      Frontier *temp = f2;
      f2 = f1;
      f1 = temp;
      if (frontier_get_total_chunks(f1) == 0)
        exploration_done = 1;

      // printf("%u \n", distance);
      // print_chunk_counts(f1);
      atomic_thread_fence(memory_order_seq_cst);
      distance++;
    }
    while (distance == old)
      ;
  }
  finalize_distances(merged_csr, thread_id);

  if (atomic_fetch_sub(&active_threads, 1) == 1) {
    // printf("Max distance: %u\n", distance);
    thread_pool_notify_parent(&tp);
  }
  return NULL;
}

void initialize_bfs(const GraphCSR *graph) {
  merged_csr = to_merged_csr(graph);
  f1 = frontier_create();
  f2 = frontier_create();
  init_thread_pool(&tp, thread_main);
  thread_pool_create(&tp);
}

void bfs(uint32_t source) {
  // Convert source vertex to mergedCSR index
  source = merged_csr->row_ptr[source];
  DISTANCE(merged_csr, source) = 0;
  frontier_push_vertex(f1, 0, source);
  exploration_done = 0;
  active_threads = MAX_THREADS;
  distance = 1;
  atomic_thread_fence(memory_order_seq_cst);
  thread_pool_start_wait(&tp);
}

uint32_t *generate_sources(const GraphCSR *graph, int runs,
                           uint32_t num_vertices, uint32_t source) {
  uint32_t *sources = malloc(runs * sizeof(uint32_t));
  if (source != UINT32_MAX) {
    for (int i = 0; i < runs; i++) {
      sources[i] = source;
    }
  } else {
    init_genrand64(SEED);
    for (int i = 0; i < runs; i++) {
      do {
        uint64_t gen = genrand64_int64();
        sources[i] = (uint32_t)gen % num_vertices;
      } while (graph->row_ptr[sources[i] + 1] - graph->row_ptr[sources[i]] ==
               0);
    }
  }
  return sources;
}

int main(int argc, char **argv) {
  Cli_Args args;
  init_cli();
  if (parse_args(argc, argv, &args) != 0) {
    return -1;
  }

  GraphCSR *graph = import_mtx(args.filename, METADATA_SIZE, VERT_MAX);
  if (graph == NULL) {
    printf("Failed to import graph from file [%s]\n", args.filename);
    return -1;
  }

  uint32_t *sources =
      generate_sources(graph, args.runs, graph->num_vertices, args.source);
  // print_sources(graph, sources, args.runs);
  printf("Threads: %d, Chunk size: %d, Vertex size: %lu bytes\n", MAX_THREADS,
         CHUNK_SIZE, sizeof(mer_t));

  distances = malloc(graph->num_vertices * sizeof(uint32_t));
  memset(distances, UINT32_MAX, graph->num_vertices * sizeof(uint32_t));
  initialize_bfs(graph);

  struct timespec start, end;
  double elapsed[args.runs];
  for (int i = 0; i < args.runs; i++) {
    clock_gettime(CLOCK_MONOTONIC, &start);
    bfs(sources[i]);
    clock_gettime(CLOCK_MONOTONIC, &end);
    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;
    elapsed[i] = seconds + nanoseconds * 1e-9;
    printf("Trial time: %16.5f\n", elapsed[i]);
    if (args.check) {
      check_bfs_correctness(graph, distances, sources[i]);
    }
    memset(distances, UINT32_MAX, merged_csr->num_vertices * sizeof(uint32_t));
  }
  // Terminate threads
  thread_pool_terminate(&tp);

  print_time(elapsed, args.runs);

  free(sources);
  free(graph->row_ptr);
  free(graph->col_idx);
  free(graph);
  destroy_frontier(f1);
  destroy_frontier(f2);
  destroy_thread_pool(&tp);
  destroy_merged_csr(merged_csr);
  free(distances);

  return 0;
}
