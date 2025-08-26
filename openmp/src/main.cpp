#include "graph.hpp"
#include <omp.h>
#include <sys/types.h>
#include "source_generator.cpp"

#define USAGE                                                                  \
  "Usage: %s <schema> <source> <implementation> <check>\nRuns BFS "            \
  "implementations. \n\nMandatory arguments:\n  <schema>\t path to dataset "   \
  "\n  <source>\t : integer. Source vertex ID "                                \
  "(64 randomly generated vertices by default) \n  <algorithm>\t : "           \
  "'merged_csr_parents', "                                                     \
  "'merged_csr_distances', reference' ('reference' by default) \n "            \
  " <check>\t : 'true', false'. Checks correctness of the result ('false' by " \
  "default)\n"

BFS_Impl *initialize_BFS(std::string filename, std::string algo_str) {
  CSR_local<uint32_t, float> *graph =
      Distr_MMIO_CSR_local_read<uint32_t, float>(filename.c_str(), false);

  if (algo_str == "merged_csr_parents") {
    return new MergedCSR_Parents(graph);
  } else if (algo_str == "merged_csr_distances") {
    return new MergedCSR_Distances(graph);
  } else {
    return new Reference(graph);
  }
}

int main(const int argc, char **argv) {
  if (argc < 2 || argc > 5) {
    printf(USAGE, argv[0]);
    return 1;
  }
  std::vector<uint32_t> sources = {};
  bool check = false;

  double t_start = omp_get_wtime();
  BFS_Impl *bfs = initialize_BFS(std::string(argv[1]), std::string(argv[3]));
  double t_end = omp_get_wtime();

  printf("Initialization: %f\n", t_end - t_start);

  if (argc > 2) {
    sources.push_back(std::stoi(argv[2]));
  } else {
    sources = generate_random_sources(bfs->graph, 64);
  }
  if (argc > 4) {
    std::string check_str = argv[4];
    if (check_str == "true") {
      check = true;
    }
  }

#pragma omp parallel
  {
#pragma omp master
    {
      printf("Number of threads: %d\n", omp_get_num_threads());
    }
  }

  uint32_t *result = new uint32_t[bfs->graph->nrows];
  // Initialize result vector
  std::fill_n(result, bfs->graph->nrows, std::numeric_limits<uint32_t>::max());

  for (uint32_t i = 0; i < sources.size(); i++) {
    t_start = omp_get_wtime();
    bfs->BFS(sources[i], result);
    t_end = omp_get_wtime();
    if (check) {
      bfs->check_result(sources[i], result);
    }
  }

  printf("Runtime: %f\n", t_end - t_start);
}