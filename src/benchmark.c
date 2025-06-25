// benchmark.c

#define _POSIX_C_SOURCE 199309L
#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// The name of the output file
#define CSV_FILENAME "benchmark_results.csv"

// --- Global state to hold benchmark data between START and END calls ---
// Note: This implementation is not thread-safe due to this global state.
static struct {
  const char *exp_name;
  int run_id;
  const char *params;
  struct timespec start_time;
} g_benchmark_data;

// --- Internal function to write data to CSV ---
static void write_to_csv(const char *exp_name, int run_id, const char *params,
                         double duration) {
  FILE *f = fopen(CSV_FILENAME, "a");
  if (f == NULL) {
    perror("Error opening benchmark CSV file");
    return;
  }

  // Check if the file is empty to write the header
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  if (size == 0) {
    fprintf(f, "experiment_name,run_id,parameters,duration\n");
  }

  // Write the data row. We quote the string fields to handle commas safely.
  fprintf(f, "\"%s\",%d,\"%s\",%.4f\n", exp_name, run_id, params, duration);

  fclose(f);
}

// --- Public functions called by macros ---

void benchmark_start_timer(const char *exp_name, int run_id,
                           const char *params) {
  g_benchmark_data.exp_name = exp_name;
  g_benchmark_data.run_id = run_id;
  g_benchmark_data.params = params;

  clock_gettime(CLOCK_MONOTONIC, &g_benchmark_data.start_time);
}

void benchmark_end_timer(void) {
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
  
  printf("Trial time: %16.5f\n", duration);

  write_to_csv(g_benchmark_data.exp_name, g_benchmark_data.run_id,
               g_benchmark_data.params, duration);
}