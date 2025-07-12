#ifndef BENCHMARK_H
#define BENCHMARK_H

/**
 * @brief Marks the beginning of a code region to be benchmarked.
 * 
 * This macro records the start time.
 * It must be paired with a corresponding BENCHMARK_END().
 */
#define BENCHMARK_START() \
    benchmark_start_timer()

/**
 * @brief Marks the end of a code region to be benchmarked.
 *
 * This macro records the end time and calculates the duration.
 */
#define BENCHMARK_END() \
    benchmark_end_timer()

/* --- Internal functions, not to be called directly by the user --- */

// Declares the function that BENCHMARK_START will call.
void benchmark_start_timer();

// Declares the function that BENCHMARK_END will call.
double benchmark_end_timer();

#endif // BENCHMARK_H