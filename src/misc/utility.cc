#include "../header/utility.h"

namespace utility {

  // construct a relative path
  std::string constructRelativePath(std::string path, std::string rootPath) {
    std::string relativePath;
    if (!(std::filesystem::path(path)).is_absolute())
      path = std::filesystem::canonical(path);

    relativePath = (std::filesystem::relative(path, rootPath)).generic_string();

    return relativePath;
  }

  std::string concatenatePath(std::string base, std::string path) {
    std::filesystem::path concatenated;
    std::filesystem::path _base(base), _path(path);
    concatenated = std::filesystem::absolute(_base / _path);

    return concatenated.generic_string();
  }

  /** 
   * retrive machine's physical time using different units
   * https://stackoverflow.com/questions/21856025/getting-an-accurate-execution-time-in-c-micro-seconds
   * https://stackoverflow.com/questions/6734375/get-current-time-in-milliseconds-using-c-and-boost
   */
  std::string getClockTime() {
    std::stringstream output;
    std::string time_micro;

    {
      uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::high_resolution_clock::now().time_since_epoch())
                        .count();
      time_micro = std::to_string(us);
    }

    {
      const auto now = std::chrono::system_clock::now();
      const auto now_as_time_t = std::chrono::system_clock::to_time_t(now);
      const auto now_us = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000000;
      output << std::put_time(std::localtime(&now_as_time_t), "%T")
             << '.' << std::setfill('0') << std::setw(3) << now_us.count() << " μ=" + time_micro.substr(10, time_micro.size()) + " ";
    }

    return output.str();
  }

}  // namespace utility

namespace utility::parse {

  // This method is used to validate and parse the string and find the right enum
  template <typename E>
  void validate(boost::any& value, const std::vector<std::string>& values, EnumOption<E>*, int) {
    boost::program_options::validators::check_first_occurrence(value);
    const std::string& valueAsString = boost::program_options::validators::get_single_string(values);

    typename std::map<std::string, E>::const_iterator it = EnumOption<E>::param_map.find(valueAsString);
    if (it == EnumOption<E>::param_map.end()) {
      throw boost::program_options::validation_error(boost::program_options::validation_error::invalid_option_value, "invalid option value");
    } else {
      value = boost::any(EnumOption<E>(it->second));
    }
  }

  // convert Mode to appropriate values
  std::istream& operator>>(std::istream& in, Mode& m) {
    std::string token;
    in >> token;
    if (token == "0")
      m = Mode::APP;
    else if (token == "1")
      m = Mode::TEST;
    else if (token == "2")
      m = Mode::BENCHMARK;
    else
      in.setstate(std::ios_base::failbit);
    return in;
  }

  template <typename T>
  boost::program_options::typed_value<T>* make_value(T* store_to) {
    return boost::program_options::value<T>(store_to);
  }

  /// @brief Makes a address given an address and port.
  Address make_address(const std::string& address_and_port) {
    // Tokenize the string on the ":" delimiter.
    std::vector<std::string> tokens;
    boost::split(tokens, address_and_port, boost::is_any_of(":"));

    // If the split did not result in exactly 2 tokens, then the value
    // is formatted wrong.
    if (2 != tokens.size()) {
      using boost::program_options::validation_error;
      throw validation_error(validation_error::invalid_option_value, "cluster.address", address_and_port);
    }

    // Create a address from the token values.
    return Address(tokens[0], boost::lexical_cast<unsigned short>(tokens[1]));
  }

  template <>
  boost::program_options::variables_map parse_options<Mode::APP>(int argc, char** argv, const std::shared_ptr<Config>& config, boost::program_options::variables_map& variables) {
    namespace po = boost::program_options;  // boost https://www.boost.org/doc/libs/1_81_0/doc/html/po.html
    namespace fs = boost::filesystem;

    EnumOption<Mode>::param_map["app"] = utility::parse::Mode::APP;
    EnumOption<Mode>::param_map["test"] = utility::parse::Mode::TEST;
    EnumOption<Mode>::param_map["benchmark"] = utility::parse::Mode::BENCHMARK;

    std::filesystem::path executablePath;
    {
      fs::path full_path(fs::initial_path<fs::path>());
      full_path = fs::system_complete(fs::path(argv[0]));
      executablePath = full_path.parent_path().string();
    }  // namespace boost::filesystem;

    po::options_description cmd_options;
    po::options_description file_options;

    try {
      { /** define program options schema */
        po::options_description generic("Generic options");
        po::options_description primary("Main program options");

        generic.add_options()("help,h", "CMD options list");
        generic.add_options()("config", po::value<std::string>(), "Configuration file");
        generic.add_options()("mode,m", po::value<EnumOption<Mode>>(), "Mode of execution: either `app`, `test`, or `benchmark`");

        primary.add_options()("port,p", po::value<unsigned short>(&config->port)->default_value(8000), "Port of RPC service");
        primary.add_options()("directory,d", po::value<std::string>(&config->directory)->default_value(utility::concatenatePath(fs::current_path().generic_string(), "tmp/server")), "Directory of data");
        primary.add_options()("cluster.address,a", make_value<std::vector<std::string>>(&config->cluster), "Addresses (incl. ports) of cluster participants <address:port>");
        primary.add_options()("flag.debug,g", po::bool_switch(&config->flag.debug)->default_value(false), "Debug flag");
        primary.add_options()("flag.leader", po::bool_switch(&config->flag.leader)->default_value(false), "testing: leader flag");
        primary.add_options()("flag.local_ubuntu", po::bool_switch(&config->flag.local_ubuntu)->default_value(false), "indicate if running locally on ubuntu, in which case the machine ip is 127.0.0.1");
        primary.add_options()("flag.timeout", po::value<int>(&config->flag.timeout)->default_value(1000), "Timeout in ms");
        primary.add_options()("flag.failrate", po::value<int>(&config->flag.failrate)->default_value(0), "Failrate: percentile");
        primary.add_options()("flag.latency", po::bool_switch(&config->flag.latency)->default_value(false), "latency flag: allow for random latency in local machine testing");
        primary.add_options()("flag.election", po::bool_switch(&config->flag.election)->default_value(true), "latency flag: allow for random latency in local machine testing");

        cmd_options.add(generic).add(primary);  // set options allowed on command line
        file_options.add(primary);              // set options allowed in config file
      }

      { /** parse & set options from different sources */
        // po::store(po::parse_command_line(argc, argv, desc), vm);
        po::store(po::command_line_parser(argc, argv).options(cmd_options).allow_unregistered().run(), variables);

        // read from configuration file
        if (variables.count("config")) {
          // string config_default = utility::concatenatePath(executablePath, "./node.ini");
          auto config_file = variables.at("config").as<std::string>();
          config->config = config_file;  // update config structure with file path
          std::ifstream ifs(utility::concatenatePath(executablePath, config_file.c_str()));
          // if (!ifs)
          //   throw std::runtime_error("can not open configuration file: " + config_file);
          if (ifs)
            po::store(po::parse_config_file(ifs, file_options, true), variables);
          ifs.close();
        }
      }

      po::notify(variables);

      // copy manually the variables:
      config->mode = (variables.count("mode")) ? variables["mode"].as<EnumOption<Mode>>().value : Mode::APP;

      if (variables.count("help")) {
        std::cout << "Distributed Replicated Database\n"
                  << cmd_options << '\n'
                  << endl;
      }

    } catch (const po::error& ex) {
      std::cerr << red << ex.what() << reset << "\n\n";
      std::cout << "(Check options with --help flag.)\n"
                << endl;

      exit(1);
    }

    return variables;
  }

  template <>
  boost::program_options::variables_map parse_options<Mode::TEST>(int argc, char** argv, const std::shared_ptr<Config>& config, boost::program_options::variables_map& variables) {
    namespace po = boost::program_options;  // boost https://www.boost.org/doc/libs/1_81_0/doc/html/po.html
    namespace fs = boost::filesystem;

    po::options_description cmd_options;
    po::options_description file_options;

    try {
      { /** define program options schema */
        po::options_description user("User program options");

        user.add_options()("target,t", po::value<std::string>()->default_value("127.0.1.1:8000"), "target address to send to");
        user.add_options()("command,c", po::value<std::string>()->default_value("get"), "command");
        user.add_options()("key,k", po::value<std::string>()->default_value("default-key"), "key");
        user.add_options()("value,v", po::value<std::string>()->default_value("default-key"), "value");

        cmd_options.add(user);   // set options allowed on command line
        file_options.add(user);  // set options allowed in config file
      }

      { /** parse & set options from different sources */
        // po::store(po::parse_command_line(argc, argv, desc), vm);
        po::store(po::command_line_parser(argc, argv).options(cmd_options).allow_unregistered().run(), variables);

        // read from configuration file
        if (variables.count("config")) {
          auto config_file = variables.at("config").as<std::string>();
          std::ifstream ifs(config_file.c_str());
          // if (!ifs)
          //   throw std::runtime_error("can not open configuration file: " + config_file);
          if (ifs)
            po::store(po::parse_config_file(ifs, file_options, true), variables);
          ifs.close();
        }
        po::notify(variables);
      }

      if (variables.count("help")) {
        std::cout << "Test binary: \n"
                  << cmd_options << '\n'
                  << endl;
        exit(0);
      }

      // TODO: ensure ip address is valid

    } catch (const po::error& ex) {
      std::cerr << red << ex.what() << reset << "\n\n";
      std::cout << "(Check options with --help flag.)\n"
                << endl;

      exit(1);
    }

    return variables;
  }

}  // namespace utility::parse

namespace utility::server {

  /**
     * start server for a specific gRPC service implementation
     *
     * @address: socket address structure with ip address & port components
    */
  template <class S>
  void server::run_gRPC_server(utility::parse::Address address) {
    // Compile-time sanity check
    static_assert(std::is_base_of<interface::RPC::Service, S>::value, "Derived not derived from BaseClass");
    S service;
    std::string a = address.address + ":" + boost::lexical_cast<std::string>(address.port);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(a, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << blue << "Server started" << reset << std::endl;
    server->Wait();
    std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << blue << "Server exited" << reset << std::endl;
  }

  template void server::run_gRPC_server<rpc::RPC>(utility::parse::Address address);  // explicit initiation - prevent linker errors for separate dec & def of template

}  // namespace utility::server
