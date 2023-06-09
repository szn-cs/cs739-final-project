#include "./common.h"

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

    app::initializeStaticInstance(config, config->cluster);  // Initialize Cluster data & Node instances

    app::init_consensus();  // run NuRaft stuff

    app::server::init_server_info();  // Initialize the server data structures

    // start a lock service server - expose that to the client and accept requests - RPC services on separate threads
    utility::parse::Address a = config->getAddress<app::Service::NODE>();
    std::thread t(utility::server::run_gRPC_server<rpc::RPC>, a);
    std::cout << blue << "⚡ lock service: " << a.toString() << reset << std::endl;

    {  // terminate app
      t.join();

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
