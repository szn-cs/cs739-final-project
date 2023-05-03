#include "test.h"

namespace test {

  void test_start_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r = app::client::start_session();

    if (r.ok()) {
      cout << green << "Test passed for this configuration." << reset << endl;
    } else {
      cout << red << "Failed to start a session with any nodes." << reset << endl;
    }
  }

  void test_single_keep_alive(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    for (const auto& [key, node] : *(app::State::memberList)) {
      cout << key << endl;

      grpc::Status r1 = app::client::start_session();
      std::pair<grpc::Status, int64_t> r2 = node->endpoint.keep_alive(app::client::info::session_id, chrono::system_clock::now() + chrono::milliseconds(6000));
      auto [status, v] = r2;

      if (status.ok()) {
        cout << "Value returned: " << v << endl;
      } else {
        cout << red << "Failed RPC" << reset << endl;
      }
    }
  }

  void test_maintain_session(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    grpc::Status r1 = app::client::start_session();
    // Idrk how to test for this without just blocking and allowing for a few rounds of keep_alives to be exchanged
    std::this_thread::sleep_for(chrono::seconds(60));
  }

  void test_create(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    return;
    // for (const auto& [key, node] : *(app::State::memberList)) {
    //   cout << key << endl;
    //   std::pair<Status, int> r = node->endpoint.open("/test", );
    //   auto [status, v] = r;

    //   if (status.ok()){
    //     cout << "Value returned: " << v << endl;
    //   }else{
    //     cout << red << "Failed RPC" << reset << endl;
    //   }
    // }
  }

  /** User execution for testing the RPC servers */
  void entrypoint(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
    //   ./target/app --mode user --command set --key k1 --value v1 --target 127.0.1.1:8002
    if (!variables.count("command"))
      throw "`command` argument is required";

    // if command exists
    std::string command = variables["command"].as<std::string>();  // defaults to `get`
    std::string target = variables["target"].as<std::string>();    // defaults to `127.0.1.1:8000`
    std::string key = variables["key"].as<std::string>();          // defaults to `default_key`
    std::string value = variables["value"].as<std::string>();      // defaults to `default_value`
    std::map<std::string, TestFnPtr_t> testFunctionMap;

    // map test functions:
    testFunctionMap["test_start_session"] = test_start_session;
    testFunctionMap["test_single_keep_alive"] = test_single_keep_alive;
    testFunctionMap["test_create"] = test_create;
    testFunctionMap["test_maintain_session"] = test_maintain_session;

    std::cout << termcolor::grey << "arguments provided: " << command + " " + key + " " + value + " " + target << termcolor::reset << std::endl;

    // handle special cases:
    if (command.compare("a command that is not of type TestFnPtr_t") == 0) {
      // do custom things...
    } else if (testFunctionMap.find(command) != testFunctionMap.end()) {
      cout << blue << "command: " << command << reset << endl;
      testFunctionMap[command](config, variables);
      return;
    }

    // fallback
    std::cout << red << "No test case found for command: " << command << reset << std::endl;
    exit(1);
  }

}  // namespace test

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
  auto f = utility::parse::parse_options<utility::parse::Mode::APP>(argc, argv, config, variables);
  if (f)
    f();  // print help info

  auto m_test = [&argc, &argv, &variables, &config]() {
    cout << termcolor::grey << "mode = TEST" << termcolor::reset << endl;
    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);

    // additional parsing
    auto f = utility::parse::parse_options<utility::parse::Mode::TEST>(argc, argv, config, variables);  // parse options from different sources
    if (f) {
      f();  // print help info
      exit(0);
    }

    return test::entrypoint(config, variables);
  };

  auto m_benchmark = [&argc, &argv, &variables, &config]() {
    cout << termcolor::grey << "mode = BENCHMARK" << termcolor::reset << endl;
    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);

    // additional parsing
    auto f = utility::parse::parse_options<utility::parse::Mode::TEST>(argc, argv, config, variables);  // parse options from different sources
    if (f)
      f();

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
