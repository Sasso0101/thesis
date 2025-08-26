#include "mmio.h"
#include <vector>
#include <random>
#include <cstdint>

// A constant seed for the random number generator, equal to kRandSeed in GAPBS.
const int kRandSeed = 27491095;

// UniDist is a utility class from the GAP Benchmark Suite for generating
// uniformly distributed random numbers within a specific range.
template <typename NodeID_, typename rng_t_,
          typename uNodeID_ = typename std::make_unsigned<NodeID_>::type>
class UniDist {
public:
  UniDist(NodeID_ max_value, rng_t_ &rng) : rng_(rng) {
    uNodeID_ u_max_value = static_cast<uNodeID_>(max_value);
    no_mod_ = (rng_.max() == u_max_value);
    mod_ = u_max_value + 1;
    uNodeID_ remainder_sub_1 = rng_.max() % mod_;
    if (remainder_sub_1 == mod_ - 1)
      cutoff_ = 0;
    else
      cutoff_ = rng_.max() - remainder_sub_1;
  }

  NodeID_ operator()() {
    uNodeID_ rand_num = rng_();
    if (no_mod_)
      return rand_num;
    if (cutoff_ != 0) {
      while (rand_num >= cutoff_)
        rand_num = rng_();
    }
    return rand_num % mod_;
  }

private:
  rng_t_ &rng_;
  bool no_mod_;
  uNodeID_ mod_;
  uNodeID_ cutoff_;
};

// Generates a vector of random source vertices for a given graph.
// It ensures that selected vertices have an out-degree greater than zero.
std::vector<uint32_t> generate_random_sources(const CSR_local<uint32_t, float>* graph, size_t num_sources) {
    std::vector<uint32_t> sources;
    sources.reserve(num_sources);
    std::mt19937_64 rng(kRandSeed);
    UniDist<uint32_t, std::mt19937_64> udist(graph->nrows - 1, rng);

    while (sources.size() < num_sources) {
        uint32_t source = udist();
        // Ensure the source has outgoing edges
        if ((graph->row_ptr[source + 1] - graph->row_ptr[source]) > 0) {
            sources.push_back(source);
        }
    }
    return sources;
}

// Example of how to use the source generator.
// You would need to replace this with your actual graph loading logic.
/*
#include "graph.hpp" // Assuming this defines CSR_local and loading functions

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph_file>" << std::endl;
        return -1;
    }
    std::string filename = argv[1];
    CSR_local<uint32_t, float> *graph =
      Distr_MMIO_CSR_local_read<uint32_t, float>(filename.c_str(), false);

    if (!graph) {
        std::cerr << "Failed to load graph: " << filename << std::endl;
        return -1;
    }

    size_t num_sources_to_generate = 64;
    std::vector<uint32_t> sources = generate_random_sources(graph, num_sources_to_generate);

    std::cout << "Generated " << sources.size() << " random source vertices:" << std::endl;
    for (uint32_t source : sources) {
        std::cout << source << std::endl;
    }

    delete graph;
    return 0;
}
*/