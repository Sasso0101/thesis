// benchmark.c

#define _POSIX_C_SOURCE 199309L
#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// --- Global state to hold benchmark data between START and END calls ---
// Note: This implementation is not thread-safe due to this global state.
static struct {
  struct timespec start_time;
} g_benchmark_data;

// --- Public functions called by macros ---

void benchmark_start_timer() {
  clock_gettime(CLOCK_MONOTONIC, &g_benchmark_data.start_time);
}

double benchmark_end_timer() {
  double duration;

  struct timespec end_time;
  clock_gettime(CLOCK_MONOTONIC, &end_time);
  // Duration in seconds
  double seconds =
      (double)(end_time.tv_sec - g_benchmark_data.start_time.tv_sec);
  // Duration in nanoseconds
  double nanoseconds =
      (double)(end_time.tv_nsec - g_benchmark_data.start_time.tv_nsec);
  // Total duration in milliseconds
  duration = (seconds) + (nanoseconds * 1e-9);
  return duration;
  
  // printf("Trial time: %16.5f\n", duration);
}