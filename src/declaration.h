#pragma once

#include <bits/stdc++.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <math.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio.hpp>
#include <boost/current_function.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <termcolor/termcolor.hpp>
#include <thread>
#include <vector>

#include "../dependency/variadic_table/include/VariadicTable.h"  // https://github.com/friedmud/variadic_table
#include "interface.grpc.pb.h"

using namespace std;
using namespace grpc;
using namespace interface;
using grpc::Server, grpc::ServerBuilder, grpc::ServerContext, grpc::ServerReader, grpc::ServerWriter, grpc::Status;  // https://grpc.github.io/grpc/core/md_doc_statuscodes.html
using termcolor::reset, termcolor::yellow, termcolor::red, termcolor::blue, termcolor::cyan, termcolor::grey, termcolor::magenta, termcolor::green;
namespace fs = std::filesystem;

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

  enum Mode {
    NODE = 0,
    USER
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
  std::istream& operator>>(std::istream& in, Mode& m);  // user to parse Mode options: convert Mode to appropriate values

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
    unsigned short port_database, port_consensus;  // RPC expotred ports
    std::string database_directory;                // directory of database data
    std::vector<std::string> cluster;              // addresses of nodes in cluster
    enum Mode mode;                                // can be either "user" or "node"; not sure how to use enums with the library instead.
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
        case app::Service::Database:
          p = this->port_database;
          break;
        case app::Service::Consensus:
          p = this->port_consensus;
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

  /// @brief Makes a address given an address and port.
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
  boost::program_options::variables_map parse_options(int argc, char** argv, const std::shared_ptr<Config>& config, boost::program_options::variables_map& variables);

}  // namespace utility::parse

namespace utility::server {
  /**
     * start server for a specific gRPC service implementation
     *
     * @address: socket address structure with ip address & port components
    */
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
    static void increment(string s) {
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

namespace rpc {
  class RPC : public interface::RPCService::Service {
   public:
  };

}  // namespace rpc

namespace app {

  template <typename C>
  struct Endpoint {
    Endpoint() = default;
    Endpoint(std::string a) : address(a), stub(std::make_shared<C>(grpc::CreateChannel(a, grpc::InsecureChannelCredentials()))) {}

    std::string address;
    std::shared_ptr<C> stub;
  };

  struct Node {
    Node() = default;  // no default constructor
    Node(std::string consensusAddress) : consensusEndpoint(Endpoint<rpc::call::ConsensusRPCWrapperCall>(consensusAddress)){};
    Node(std::string consensusAddress, std::string databaseAddress) : consensusEndpoint(Endpoint<rpc::call::ConsensusRPCWrapperCall>(consensusAddress)),
                                                                      databaseEndpoint(Endpoint<rpc::call::DatabaseRPCWrapperCall>(databaseAddress)){};
    Node(utility::parse::Address consensusAddress) : Node(consensusAddress.toString()){};
    Node(utility::parse::Address consensus, utility::parse::Address database) : Node(consensus.toString(), database.toString()){};

    Endpoint<rpc::call::ConsensusRPCWrapperCall> consensusEndpoint;
    Endpoint<rpc::call::DatabaseRPCWrapperCall> databaseEndpoint;
  };

  struct Cluster {
    static std::shared_ptr<std::map<std::string, std::shared_ptr<Node>>> memberList;  // addresses of nodes in cluster
    static std::shared_ptr<utility::parse::Config> config;
    static std::shared_ptr<Node> currentNode;  // current machine's Node object

    // static void getLeader();  // return tuple(bool, Node)

    static std::string leader;
    static pthread_mutex_t leader_mutex;
  };

  void initializeStaticInstance(std::vector<std::string> addressList, std::shared_ptr<utility::parse::Config> config);

}  // namespace app
