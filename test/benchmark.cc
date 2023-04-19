#include "benchmark/benchmark.h"

#include <chrono>
#include <string>

#include "../src/declaration.h"

// This test is not fully automated and required manual setup of cluster beforehand.
void run(rpc::call::DatabaseRPCWrapperCall& c, const size_t size) {
  grpc::Status res;
  benchmark::DoNotOptimize(res = c.set("k", "v"));
  benchmark::ClobberMemory();

  if (!res.ok())
    throw std::runtime_error("RPC FAILURE");
}

static void benchmark_function(benchmark::State& state) {
  state.PauseTiming();
  cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
  string address = "127.0.1.1:9005";  // can be leader / follower depending on initial setup (not automated)
  rpc::call::DatabaseRPCWrapperCall c{grpc::CreateChannel(address, grpc::InsecureChannelCredentials())};

  state.ResumeTiming();

  // Perform setup here
  for (auto _ : state) {
    // This code gets timed
    run(c, 1);
  }
}

// Register the function as a benchmark
// BENCHMARK(benchmark_function)->Arg(1);  // ->Arg(200000)->Arg(400000);
BENCHMARK(benchmark_function)->Iterations(pow(10, 4));  // ->Arg(200000)->Arg(400000)

// BENCHMARK(benchmark_function)->RangeMultiplier(2)->Range(1 << 10, 1 << 20);

/** 
 * initialize google/benchmark main with custom code
 * 
 * Alternatively run the benchmark:
 * BENCHMARK_MAIN(); // google/benchmark main macro
*/
int main(int argc, char** argv) {
  cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;

  std::shared_ptr<utility::parse::Config> config = std::make_shared<utility::parse::Config>();
  boost::program_options::variables_map variables;

  utility::parse::parse_options<utility::parse::Mode::NODE>(argc, argv, config, variables);  // parse options from different sources

  // Initialize Cluster data & Node instances
  app::initializeStaticInstance(config);
}

::benchmark::Initialize(&argc, argv);
::benchmark::RunSpecifiedBenchmarks();
}
