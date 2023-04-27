#pragma once

#include "./library.h"
#include "./rpc.h"

namespace app {
  enum Service {
    NODE
  };
}

namespace utility {
  // construct a relative path
  std::string constructRelativePath(std::string path, std::string rootPath);
  std::string concatenatePath(std::string base, std::string path);

  /** 
   * retrive machine's physical time using different units
   * https://stackoverflow.com/questions/21856025/getting-an-accurate-execution-time-in-c-micro-seconds
   * https://stackoverflow.com/questions/6734375/get-current-time-in-milliseconds-using-c-and-boost
  */
  std::string getClockTime();
}  // namespace utility

namespace utility::parse {

  enum Mode {
    APP = 0,
    TEST = 1,
    BENCHMARK = 2
  };

  /// @brief Address type that contains an address and port.
  struct Address {
    std::string address;
    unsigned short port;

    std::string toString() {
      return this->address + ":" + std::to_string(this->port);
    }

    /// @brief Constructor.
    Address(std::string address, unsigned short port)
        : address(address),
          port(port) {}

    /// @brief Stream insertion operator for address.
    ///
    /// @param stream The stream into which address is being inserted.
    /// @param s The address object.
    ///
    /// @return Reference to the ostream.
    friend std::ostream& operator<<(std::ostream& stream, const Address& n) {
      return stream << "address: " << n.address
                    << ", port: " << n.port;
    }
  };

  // support for enum with boost::program_options package:
  // This class contains the value of the parsed option
  // It also contains the map from string to enum used to know wich is wich
  template <typename E>
  struct EnumOption {
    EnumOption() : value(static_cast<E>(0)) {}
    EnumOption(const E& newValue) : value(newValue) {}

    E value;
    static std::map<std::string, E> param_map;
  };

  template <typename E>
  std::map<std::string, E> EnumOption<E>::param_map;

  template <typename E>
  void validate(boost::any& value, const std::vector<std::string>& values, EnumOption<E>*, int);
  std::istream& operator>>(std::istream& in, Mode& m);  // used to parse Mode options: convert Mode to appropriate values

  /**
     * TODO: modify configurations
     * Configuration info
     *
     * options input priority/hierarchy:
     *  1. command-line arguments
     *  2. configuration file
     *  3. default hardcoded values
    */
  struct Config {        // Declare options that will be allowed both on command line and in config file
    std::string config;  // configuration file path that is being read
    std::string ip;
    unsigned short port;               // RPC expotred ports
    std::string directory;             // directory of database data
    std::vector<std::string> cluster;  // addresses of nodes in cluster
    enum Mode mode;                    // can be either "test" or "app"; not sure how to use enums with the library instead.
    struct flag {
      bool debug;  // debug flag
      bool leader;
      bool local_ubuntu;
      bool latency;
      bool election;  // trigger election on strart-up
      int timeout;
      int failrate;
    } flag;

    // TODO: fail rate, and testing configs
    // TODO: if needed
    // set acceptor list -> vector<pair<int, std::string>>; pairs of server IDs with their addresses
    // set leader -> pair<int, string>

    /** return local address with port of service (database or consensus) */
    template <app::Service s>
    Address getAddress() {
      unsigned short p;

      switch (s) {
        case app::Service::NODE:
        default:
          p = this->port;
          break;
      }

      return Address{this->ip, boost::lexical_cast<unsigned short>(p)};
    }

    /** get machines ip address (assumes a single local ip / network card) */
    static std::string getLocalIP() {
      namespace ip = boost::asio::ip;

      boost::asio::io_service ioService;
      ip::tcp::resolver resolver(ioService);

      return resolver.resolve(ip::host_name(), "")->endpoint().address().to_string();
    }

    Config() : ip(Config::getLocalIP()){};
  };

  Address make_address(const std::string& address_and_port);

  /// @brief Convenience function for when a 'store_to' value is being provided to typed_value.
  ///
  /// @param store_to The variable that will hold the parsed value upon notify.
  ///
  /// @return Pointer to a type_value.
  template <typename T>
  boost::program_options::typed_value<T>* make_value(T* store_to);

  /**
     * https://download.cosine.nl/gvacanti/parsing_configuration_files_c++_CVu265.pdf
     *
    */
  template <Mode mode>
  std::function<void()> parse_options(int argc, char** argv, const std::shared_ptr<Config>& config, boost::program_options::variables_map& variables);

}  // namespace utility::parse

namespace utility::server {
  template <class S>
  void run_gRPC_server(utility::parse::Address address);
};  // namespace utility::server

namespace utility::debug {
  struct Statistics {
    enum rpc_type {
      incoming,
      outgoing
    };
    // count RPCs
    static std::map<std::string, int> incount;
    static std::map<std::string, int> outcount;

    // increment the corresponding element and initialize if necessary
    template <rpc_type t>
    static void increment(std::string s) {
      switch (t) {
        case rpc_type::incoming:
          incount[s] = (incount.find(s) == incount.end()) ? 1 : incount[s] + 1;
          break;
        case rpc_type::outgoing:
          outcount[s] = (outcount.find(s) == outcount.end()) ? 1 : outcount[s] + 1;
          break;
      }
    }

    // print results
    template <rpc_type t>
    static void print() {
      std::map<std::string, int> l;

      switch (t) {
        case rpc_type::incoming:
          l = incount;
          break;
        case rpc_type::outgoing:
          l = outcount;
          break;
      }

      std::cout << green << "registered rpc call counts: [" << reset;
      std::vector<string> key_l;
      std::vector<int> value_l;
      for (std::map<std::string, int>::iterator it = l.begin(); it != l.end(); ++it) {
        key_l.push_back(it->first);
        value_l.push_back(it->second);
        std::cout << green << ", " << it->first << reset;
      }
      std::cout << "]" << endl;
    }
  };
}  // namespace utility::debug
