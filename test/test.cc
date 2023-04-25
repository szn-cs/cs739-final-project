#include "./test.h"

namespace benchmark {
  // This test is not fully automated and required manual setup of cluster beforehand.
  void run(rpc::Endpoint& c, const size_t size) {
    std::pair<grpc::Status, int> t;
    benchmark::DoNotOptimize(t = c.func(123456));
    auto [res, r] = t;
    benchmark::ClobberMemory();

    if (!res.ok())
      throw std::runtime_error("RPC FAILURE");
  }

  static void function(benchmark::State& state) {
    state.PauseTiming();
    cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
    std::string address = "127.0.1.1:8000";  // can be leader / follower depending on initial setup (not automated)
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

namespace test {

  void test_1(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    cout << blue << "command: test_1" << reset << endl;

    for (const auto& [key, node] : *(app::State::memberList)) {
      std::pair<Status, int> r = node->endpoint.func(123456);
      auto [status, v] = r;

      if (status.ok())
        cout << "Value returned: " << v << endl;
      else
        cout << red << "Failed RPC" << reset << endl;
    }
  }

  /** User execution for testing the RPC servers */
  void entrypoint(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    //   ./target/app --mode user --command set --key k1 --value v1 --target 127.0.1.1:8002
    if (!variables.count("command"))
      throw "`command` argument is required";

    // if command exists
    std::string command = variables["command"].as<std::string>();
    std::string target = variables["target"].as<std::string>();
    std::string key = variables["key"].as<std::string>();
    std::string value = variables["value"].as<std::string>();

    std::cout << termcolor::grey << "arguments provided: " << command + " " + key + " " + value + " " + target << termcolor::reset << std::endl;

    if (command.compare("test_1") == 0) {
      test_1(config, variables);
    } else if (command.compare("test_2") == 0) {
      // test_1(config, variables);
    } else if (command.compare("test_3") == 0) {
      // test_1(config, variables);
    } else {
      cout << red << "No command matches " << command << reset << endl;
    }
  }

}  // namespace test

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

  // parse options from different sources
  utility::parse::parse_options<utility::parse::Mode::APP>(argc, argv, config, variables);

  auto m_test = [&argc, &argv, &variables, &config]() {
    cout << termcolor::grey << "mode = TEST" << termcolor::reset << endl;
    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);

    // additional parsing
    utility::parse::parse_options<utility::parse::Mode::TEST>(argc, argv, config, variables);  // parse options from different sources

    return test::entrypoint(config, variables);
  };

  auto m_benchmark = [&argc, &argv, &variables, &config]() {
    cout << termcolor::grey << "mode = BENCHMARK" << termcolor::reset << endl;
    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);

    // additional parsing
    utility::parse::parse_options<utility::parse::Mode::TEST>(argc, argv, config, variables);  // parse options from different sources

    if (config->flag.debug)
      cout << termcolor::grey << "Using config file at: " << config->config << termcolor::reset << endl;

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
  };

  /** pick Mode of opeartion: either run distributed database or run the user testing part. */
  switch (config->mode) {
    case utility::parse::Mode::BENCHMARK:
      m_benchmark();
      break;
    case utility::parse::Mode::TEST:
    default:
      m_test();
      break;
  }

  return 0;
}
