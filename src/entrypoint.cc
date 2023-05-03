#include "./header/common.h"

/**
 * initialize configurations, run RPC servers, and start consensus coordination
 */
int main(int argc, char* argv[]) {
  cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
  boost::program_options::variables_map variables;
  std::shared_ptr<utility::parse::Config> config = std::make_shared<utility::parse::Config>();

  // parse options from different sources
  auto f = utility::parse::parse_options<utility::parse::Mode::APP>(argc, argv, config, variables);
  if (f) {
    f();  // print help info
    exit(0);
  }

  auto m_app = [&argc, &argv, &variables, &config]() {
    cout << termcolor::grey << "mode = APP" << termcolor::reset << endl;

    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config, config->cluster);

    // handle directory:
    fs::create_directories(fs::absolute(config->directory));  // create database direcotry directory if doesn't exist

    // Initialize the server data structures
    app::server::init_server_info();
    
    // RPC services on separate threads
    utility::parse::Address a = config->getAddress<app::Service::NODE>();
    std::thread t(utility::server::run_gRPC_server<rpc::RPC>, a);
    std::cout << termcolor::blue << "⚡ service: " << a.toString() << termcolor::reset << std::endl;

    // call app functionality
    // TODO:

    t.join();
  };

  switch (config->mode) {
    case utility::parse::Mode::APP:      
    default:
      m_app();
      break;
  }

  cout << termcolor::grey << utility::getClockTime() << "Node exited" << termcolor::reset << endl;
  return 0;
}
