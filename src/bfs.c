#include <stdlib.h>
#include <string.h>
#define _GNU_SOURCE
#include "cli_parser.h"
#include "benchmark.h"
#include "config.h"
#include "debug_utils.h"
#include "frontier.h"
#include "merged_csr.h"
#include "mt19937-64.h"
#include "thread_pool.h"
#include "mmio_c_wrapper.h"
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

thread_pool_t tp;

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

void initialize_bfs(const mmio_csr_u32_f32_t *graph) {
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

uint32_t *generate_sources(const mmio_csr_u32_f32_t *graph, int runs,
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

typedef struct {
  char *filename; // Will be allocated by the parser
  int runs;
  int source_id;
  bool check;
  bool output;
} AppArgs;

int main(int argc, char **argv) {
  AppArgs args = {.filename = NULL,
                  .runs = 1,
                  .source_id = -1,
                  .check = false,
                  .output = true};
  const CliOption options[] = {
      {'f', "file", "Load graph from file", ARG_TYPE_STRING, &args.filename,
       true},
      {'n', "runs", "Number of runs", ARG_TYPE_INT, &args.runs, false},
      {'s', "source", "ID of source vertex", ARG_TYPE_INT, &args.source_id,
       false},
      {'c', "check", "Checks BFS correctness", ARG_TYPE_BOOL, &args.check,
       false}};
  int num_options = sizeof(options) / sizeof(options[0]);
  const char *app_description =
      "An optimized BFS algorithm for large-diameter graphs.";
  int parse_result =
      cli_parse(argc, argv, options, num_options, app_description);

  // Check the result: 0 is success, 1 means help was printed, -1 is an error.
  if (parse_result != 0) {
    if (args.filename)
      free(args.filename);
    return (parse_result == 1) ? 0 : 1;
  }

  // const char* max_threads_env = getenv("MAX_THREADS");
  // int max_threads = MAX_THREADS;
  // if (max_threads_env) {
  //   int val = atoi(max_threads_env);
  //   if (val > 0) {
  //     max_threads = val;
  //     printf("Overriding MAX_THREADS: %d (from environment variable)\n", max_threads);
  //   }
  // }
  // #undef MAX_THREADS
  // #define MAX_THREADS max_threads

  // GraphCSR *graph = import_mtx(args.filename, METADATA_SIZE, VERT_MAX);
  mmio_csr_u32_f32_t *graph = mmio_read_csr_u32_f32(args.filename, false);
  if (graph == NULL) {
    printf("Failed to import graph from file [%s]\n", args.filename);
    return -1;
  }

  uint32_t *sources =
      generate_sources(graph, args.runs, graph->nrows, args.source_id);

  distances = malloc(graph->nrows * sizeof(uint32_t));
  memset(distances, UINT32_MAX, graph->nrows * sizeof(uint32_t));
  initialize_bfs(graph);

  static char param_buffer[256];

  for (int i = 0; i < args.runs; i++) {
    BENCHMARK_START("BFS", i, param_buffer);
    bfs(sources[i]);
    BENCHMARK_END();

    // snprintf(param_buffer, sizeof(param_buffer), "dataset=%s,threads=%d,chunk_size=%d", args.filename, MAX_THREADS, CHUNK_SIZE);
    uint32_t diameter = 0;
    for (mer_t j = 0; j < graph->nrows; j++) {
      if (distances[j] > diameter)
        diameter = distances[j];
    }
    snprintf(param_buffer, sizeof(param_buffer), "diameter=%d,threads=%d,chunk_size=%d", diameter, MAX_THREADS, CHUNK_SIZE);

    if (args.check) {
      check_bfs_correctness(graph, distances, sources[i]);
    }

    memset(distances, UINT32_MAX, merged_csr->num_vertices * sizeof(uint32_t));
  }
  // Terminate threads
  thread_pool_terminate(&tp);

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
