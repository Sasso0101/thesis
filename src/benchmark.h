// benchmark.h

#ifndef BENCHMARK_H
#define BENCHMARK_H

/**
 * @brief Marks the beginning of a code region to be benchmarked.
 * 
 * This macro records the start time and the metadata for the benchmark run.
 * It must be paired with a corresponding BENCHMARK_END().
 *
 * @param exp_name A string literal for the name of the experiment.
 * @param run_id An integer identifier for this specific run.
 * @param params A string literal containing arbitrary text, such as parameters.
 *               This string will be quoted in the CSV to handle commas.
 */
#define BENCHMARK_START(exp_name, run_id, params) \
    benchmark_start_timer(exp_name, run_id, params)

/**
 * @brief Marks the end of a code region to be benchmarked.
 *
 * This macro records the end time, calculates the duration, and writes
 * the complete record (name, id, params, duration) to the CSV file.
 */
#define BENCHMARK_END() \
    benchmark_end_timer()

/* --- Internal functions, not to be called directly by the user --- */

// Declares the function that BENCHMARK_START will call.
void benchmark_start_timer(const char* exp_name, int run_id, const char* params);

// Declares the function that BENCHMARK_END will call.
void benchmark_end_timer(void);

#endif // BENCHMARK_H