#include "./common.h"

/** 
 * initialize google/benchmark main with custom code
 * 
 * Alternatively run the benchmark:
 * BENCHMARK_MAIN(); // google/benchmark main macro
*/
namespace benchmark {
  // This test is not fully automated and required manual setup of cluster beforehand.
  void run(rpc::Endpoint& c, const size_t size) {
    grpc::Status t;
    benchmark::DoNotOptimize(t = c.ping());
    benchmark::ClobberMemory();

    if (!t.ok())
      throw std::runtime_error("RPC FAILURE");
  }

  static void function(benchmark::State& state) {
    state.PauseTiming();
    cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
    std::string address = "localhost:8000";  // can be leader / follower depending on initial setup (not automated)
    rpc::Endpoint c{address};

    state.ResumeTiming();

    // Perform setup here
    for (auto _ : state) {
      // This code gets timed
      run(c, 1);
    }
  }

  // Register the function as a benchmark
  // BENCHMARK(benchmark_function)->RangeMultiplier(2)->Range(1 << 10, 1 << 20);
  // BENCHMARK(benchmark_function)->Arg(1);  // ->Arg(200000)->Arg(400000);
  BENCHMARK(benchmark::function)->Iterations(pow(10, 4));  // ->Arg(200000)->Arg(400000)
}  // namespace benchmark