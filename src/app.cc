#include "./header/common.h"

namespace rpc {

  /************************ ping rpcs TODO: These aren't needed ************************/
  Status RPC::ping(ServerContext* context, const interface::Empty* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "RPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    return Status::OK;
  }

  grpc::Status Endpoint::ping() {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    Empty request;
    Empty response;

    if (app::State::config->flag.debug) cout << yellow << "calling RPC::func @ " << this->address << " endpoint;" << reset << endl;
    grpc::Status status = this->stub->ping(&context, request, &response);

    return status;
  }
}

namespace rpc{
  /************************ init_session rpcs ************************/
  Status RPC::init_session(ServerContext* context, const interface::InitSessionRequest* request, interface::Empty* response){
    if (app::State::config->flag.debug) {
      const std::string className = "RPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    if((*(app::State::master)).compare(app::State::config->getAddress<app::Service::NODE>().toString()) == 0){
      return Status::OK;
    }
    return Status::CANCELLED;
  }

  grpc::Status Endpoint::init_session() {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    InitSessionRequest request;
    Empty response;


    grpc::Status status = this->stub->init_session(&context, request, &response);

    return status;
  }
}

namespace rpc{
  /************************ keep_alive rpcs ************************/
  grpc::Status RPC::keep_alive(ServerContext* context, const interface::KeepAliveRequest* request, interface::KeepAliveResponse* response){
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    if(context->IsCancelled()){
      return Status(StatusCode::CANCELLED, "Exceeded deadline.");
    }

    return Status::OK;
  }

  std::pair<grpc::Status, int32_t> Endpoint::keep_alive(std::string client_id, chrono::system_clock::time_point deadline){
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    KeepAliveRequest request;
    KeepAliveResponse response;

    // Set deadline
    context.set_deadline(deadline);

    grpc::Status status = this->stub->keep_alive(&context, request, &response);

    std::pair<grpc::Status, int32_t> res;
    res.first = status;
    res.second = 0; // TODO: Will implement later

    return res;
  }


  std::pair<grpc::Status, int32_t> Endpoint::keep_alive(std::string client_id, std::map<std::string, LockStatus> locks, chrono::system_clock::time_point deadline){
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    KeepAliveRequest request;
    KeepAliveResponse response;

    // Set deadline
    context.set_deadline(deadline);

    grpc::Status status = this->stub->keep_alive(&context, request, &response);

    std::pair<grpc::Status, int32_t> res;
    res.first = status;
    res.second = 0; // TODO: Will implement later

    return res;
  }

}  // namespace rpc

/**
 * @brief Setup/initialization for a server
 * 
 */
namespace app {

  std::shared_ptr<std::map<std::string, std::shared_ptr<Node>>> State::memberList = nullptr;
  std::shared_ptr<utility::parse::Config> State::config = nullptr;
  std::shared_ptr<Node> State::currentNode = nullptr;
  std::shared_ptr<std::string> State::master = nullptr;

  void initializeStaticInstance(std::shared_ptr<utility::parse::Config> config, std::vector<std::string> addressList) {
    State::config = config;

    State::memberList = std::make_shared<std::map<std::string, std::shared_ptr<Node>>>();

    // Transform each config into a address via make_address, inserting each object into the vector.
    std::vector<utility::parse::Address> l;
    std::transform(addressList.begin(), addressList.end(), std::back_inserter(l), utility::parse::make_address);
    for (utility::parse::Address a : l) {
      if (app::State::config->flag.debug) cout << grey << "registered cluster address: " << a.toString() << reset << endl;
      State::memberList->insert(std::make_pair(a.toString(), std::make_shared<Node>(a.toString())));
    }

    utility::parse::Address selfAddress = State::config->getAddress<app::Service::NODE>();
    auto iterator = State::memberList->find(selfAddress.toString());
    if (iterator == State::memberList->end()) {  // not found
      State::currentNode = std::make_shared<Node>(selfAddress);
      State::memberList->insert(std::make_pair(selfAddress.toString(), State::currentNode));
    } else {
      State::currentNode = iterator->second;
    }

    if (config->flag.debug) {
      std::cout << termcolor::grey << "Size of cluster (including self): " << State::memberList->size() << reset << std::endl;
      cout << termcolor::grey << "Using config file at: " << config->config << termcolor::reset << endl;
    }

    // Find leader
    if(config->flag.leader){
      // We are leader, used mainly for testing
      if(config->flag.debug){
        std::cout << termcolor::yellow << "We are leader" << termcolor::reset << std::endl;
      }
      State::master = std::make_shared<std::string>(selfAddress.toString());
    }else{
      // Run Paxos to find master
      State::master = std::make_shared<std::string>("");
    }
  }

}  // namespace app

namespace app::server {

} // namespace app::server

namespace app::client {
  std::string info::session_id;
  std::thread info::session;              
  chrono::system_clock::time_point info::lease_start;                   
  chrono::milliseconds info::lease_length;                    
  std::shared_ptr<map<std::string, LockStatus>> info::locks; 
  bool info::jeopardy;                           
  bool info::expired;
  std::shared_ptr<Node> info::master;


  grpc::Status start_session(){
    using namespace std::chrono;

    // Session ID is simply the client's ip:port, acting as a simple uniquifier
    info::session_id = app::State::config->getAddress<app::Service::NODE>().toString();
    info::locks = std::make_shared<std::map<std::string, LockStatus>>();
    info::jeopardy = false;
    info::expired = false;
    info::lease_length = milliseconds(utility::DEFAULT_LEASE_DURATION);
    info::master = nullptr;

    // Lease start is in seconds since epoch
    info::lease_start =  system_clock::now();

    // Try to establish session with all nodes in cluster
    for (const auto& [key, node] : *(State::memberList)) {
      grpc::Status status = node->endpoint.init_session();

      if(!status.ok()){ // If server is down or replies with grpc::StatusCode::ABORTED
        if(State::config->flag.debug){
          cout << termcolor::grey << "Node " << key << " replied with an error." << termcolor::reset << endl;
          continue;
        }
      }

      // Session established with server
      info::master = node;
      break;
    }

    if(info::master == nullptr){
      if(State::config->flag.debug){
        cout << termcolor::red << "Could not establish a session with any server." << termcolor::reset << endl;
      }
      return Status::CANCELLED;
    }

    return Status::OK;
  }
} // namespace app::client
