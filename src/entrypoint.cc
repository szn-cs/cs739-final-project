#include "./common.h"

void example_usage_of_consensus() {
  {  // EXAMPLE: add sever dynamically to NuRaft
    using namespace app::consensus;

    // const std::vector<std::string>& tokens = {"2", "localhost:9002"};
    // add_server(tokens);
  }

  {  // EXAMPLE: commit a command value to consensus log every 2 seconds and print cluster state
    using namespace app::consensus;

    while (true) {
      if (app::State::config->flag.debug) {
        print_status();
        server_list();
      }

      cout << grey << "⏳ wait 2s" << reset << endl;
      sleep(2);

      cout << on_bright_cyan << "🧬 Trying to replicate command `+123`" << reset << endl;
      const std::vector<std::string>& tokens = {"+123"};
      append_log("+", tokens);
    }
  }
}

/**
 * initialize configurations, run RPC servers, and start consensus coordination
 */
int main(int argc, char* argv[]) {
  cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
  boost::program_options::variables_map variables;
  std::shared_ptr<utility::parse::Config> config = std::make_shared<utility::parse::Config>();
  std::vector<std::string> args(argv, argv + argc);
  std::vector<char*> new_argv;

  // parse options from different sources
  utility::parse::parse_options<utility::parse::Parse::GENERIC>(argc, argv, config, variables);
  auto f = utility::parse::parse_options<utility::parse::Parse::APP>(argc, argv, config, variables);
  remove_command_argument(argc, argv, config, variables, args, new_argv);  // remove `mode` from argv
  if (f) {
    f();  // print help info
    exit(0);
  }

  auto m_app = [&argc, &argv, &variables, &config]() {
    cout << grey << "mode = APP" << reset << endl;

    // handle directory:
    fs::create_directories(fs::absolute(config->directory));  // create directory if doesn't exist

    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);

    // run NuRaft stuff
    app::init_consensus();

    // EXAMPLE of NuRaft consensus
    example_usage_of_consensus();

    // Initialize the server data structures
    app::server::init_server_info();

    // RPC services on separate threads
    utility::parse::Address a = config->getAddress<app::Service::NODE>();
    std::thread t(utility::server::run_gRPC_server<rpc::RPC>, a);
    std::cout << blue << "⚡ service: " << a.toString() << reset << std::endl;

    // call app functionality
    // TODO:
    // start a lock service server - expose that to the client and accept requests

    t.join();

    {  // terminate app
      // gracefully terminate NuRaft & ASIO used
      app::State::stuff.launcher_.shutdown(5);
      app::State::stuff.reset();
    }
  };

  switch (config->mode) {
    case utility::parse::Mode::APP:
    default:
      m_app();
      break;
  }

  cout << grey << utility::getClockTime() << "Node exited" << reset << endl;
  return 0;
}
