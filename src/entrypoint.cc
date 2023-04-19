#include "./declaration.h"

int user_entrypoint(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables);

/**
 * initialize configurations, run RPC servers, and start consensus coordination
 */
int main(int argc, char* argv[]) {
  cout << termcolor::grey << utility::getClockTime() << termcolor::reset << endl;
  boost::program_options::variables_map variables;
  std::shared_ptr<utility::parse::Config> config = std::make_shared<utility::parse::Config>();

  auto m_user = [&variables, &config]() {
    // parse options from different sources
    utility::parse::parse_options<utility::parse::Mode::NODE>(argc, argv, config, variables);

    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config);

    // additional parsing
    utility::parse::parse_options<utility::parse::Mode::USER>(argc, argv, config, variables);  // parse options from different sources
    return user_entrypoint(config, variables);
  };

  auto m_node = [&variables, &config]() {
    // parse options from different sources
    utility::parse::parse_options<utility::parse::Mode::NODE>(argc, argv, config, variables);

    // Initialize Cluster data & Node instances
    app::initializeStaticInstance(config);

    // handle directory:
    fs::create_directories(fs::absolute(config->database_directory));  // create database direcotry directory if doesn't exist

    // RPC services on separate threads
    utility::parse::Address a1 = config->getAddress<app::Service::Consensus>();
    std::thread t1(utility::server::run_gRPC_server<rpc::RPC>, a1);
    std::cout << termcolor::blue << "⚡ service: " << a1.toString() << termcolor::reset << std::endl;

    // call app functionality
    // TODO:

    t1.join();
  };

  /** pick Mode of opeartion: either run distributed database or run the user testing part. */
  switch (config->mode) {
    case utility::parse::Mode::USER:
      m_user();
      break;
    case utility::parse::Mode::NODE:
    default:
      m_node();
  }

  cout << termcolor::grey << utility::getClockTime() << "Node exited" << termcolor::reset << endl;
  return 0;
}

/** User execution for testing the RPC servers */
void user_entrypoint(std::shared_ptr<utility::parse::Config> config, boost::program_options::variables_map& variables) {
  //   ./target/app --mode user --command set --key k1 --value v1 --target 127.0.1.1:8002
  if (!variables.count("command"))
    throw "`command` argument is required";

  // if command exists
  auto command = variables["command"].as<std::string>();
  auto target = variables["target"].as<std::string>();
  auto key = variables["key"].as<std::string>();
  auto value = variables["value"].as<std::string>();

  std::cout << "arguments provided: " << command + " " + key + " " + value + " " + target << std::endl;

  switch (command) {
    case "test1":
      // call some function
      break;

    case "test2":
      // call some function
      break;

    case "test3":
      // call some function
      break;
  }
}
