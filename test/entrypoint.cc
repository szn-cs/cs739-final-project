#include "./common.h"

int main(int argc, char** argv) {
  cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
  std::shared_ptr<utility::parse::Config> config = std::make_shared<utility::parse::Config>();
  boost::program_options::variables_map variables;
  std::vector<std::string> args(argv, argv + argc);
  std::vector<char*> new_argv;

  // parse options from different sources
  utility::parse::parse_options<utility::parse::Parse::GENERIC>(argc, argv, config, variables);
  utility::parse::parse_options<utility::parse::Parse::APP>(argc, argv, config, variables);

  auto m_test = [&argc, &argv, &variables, &config, &args, &new_argv]() {
    using namespace test;
    cout << termcolor::grey << "mode = TEST" << termcolor::reset << endl;

    // additional parsing
    auto f = utility::parse::parse_options<utility::parse::Parse::TEST>(argc, argv, config, variables);  // parse options from different sources
    if (f) {
      f();  // print help info
      exit(0);
    }

    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);
    remove_command_argument(argc, argv, config, variables, args, new_argv);  // remove `mode` from argv

    //   ./target/app --mode test --command set --key k1 --value v1 --target 127.0.1.1:8002
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
    testFunctionMap["test_create"] = test_create;
    testFunctionMap["test_delete"] = test_delete;
    testFunctionMap["test_acquire"] = test_acquire;
    testFunctionMap["test_2_clients"] = test_2_clients;
    testFunctionMap["test_2_clients_create"] = test_2_clients_create;
    testFunctionMap["test_release"] = test_release;
    testFunctionMap["test_read_shared"] = test_read_shared;
    testFunctionMap["test_read_exclusive"] = test_read_exclusive;
    testFunctionMap["test_write_exclusive"] = test_write_exclusive;
    testFunctionMap["test_write_shared"] = test_write_shared;
    testFunctionMap["test_rw"] = test_rw;
    testFunctionMap["close_session"] = close_session;

    std::cout << termcolor::grey << "arguments provided: " << command + " " + key + " " + value + " " + target << termcolor::reset << std::endl;

    // handle special cases:
    if (command.compare("a command that is not of type TestFnPtr_t") == 0) {
      // do custom things...
    } else if (testFunctionMap.find(command) != testFunctionMap.end()) {
      cout << blue << "command: " << command << reset << endl;
      testFunctionMap[command](config, variables);
      return;
    } else {
      // fallback
      std::cout << red << "No test case found for command: " << command << reset << std::endl;
      exit(1);
    }
  };

  auto m_benchmark = [&argc, &argv, &variables, &config, &args, &new_argv]() {
    using namespace benchmark;

    cout << termcolor::grey << "mode = BENCHMARK" << termcolor::reset << endl;
    cout << grey << "NOTE: benchmark mode should be run with release compilation option (non-debug)" << reset << endl; 

    // additional parsing
    auto f = utility::parse::parse_options<utility::parse::Parse::TEST>(argc, argv, config, variables);  // parse options from different sources
    if (f)
      f();

    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);
    remove_command_argument(argc, argv, config, variables, args, new_argv);  // remove `mode` from argv

    if (config->flag.debug)
      cout << termcolor::grey << "Using config file at: " << config->config << termcolor::reset << endl;

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
  };

  auto m_interactive = [&argc, &argv, &variables, &config, &args, &new_argv]() {
    using namespace interactive;

    // additional parsing
    auto f = utility::parse::parse_options<utility::parse::Parse::TEST>(argc, argv, config, variables);  // parse options from different sources
    if (f) {
      f();  // print help info
      exit(0);
    }

    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);
    remove_command_argument(argc, argv, config, variables, args, new_argv);  // remove `mode` from argv

    std::cout << "    -- Distributed Lock Service --" << std::endl;
    std::cout << "                         Version 0.1.0" << std::endl;

    utility::prompt::loop("Lock Service", 1 /*client identifier*/, &do_cmd);
    return 0;
  };

  /** pick Mode of opeartion: either run distributed database or run the user testing part. */
  switch (config->mode) {
    case utility::parse::Mode::BENCHMARK:
      m_benchmark();
      break;
    case utility::parse::Mode::INTERACTIVE:
      m_interactive();
      break;
    case utility::parse::Mode::TEST:
    default:
      m_test();
      break;
  }

  return 0;
}
