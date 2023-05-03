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
      return app::server::create_session(request->client_id());
    }
    return Status::CANCELLED;
  }

  grpc::Status Endpoint::init_session(std::string client_id) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    InitSessionRequest request;
    Empty response;

    request.set_client_id(client_id);

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

  // This method does initializing of information common to both servers and clients
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

    if (config->flag.debug) {
      std::cout << termcolor::grey << "Size of cluster: " << State::memberList->size() << reset << std::endl;
      cout << termcolor::grey << "Using config file at: " << config->config << termcolor::reset << endl;
    }

  }

}  // namespace app


namespace app::server {
  std::shared_ptr<std::map<std::string, std::shared_ptr<Lock>>> info::locks = nullptr;
  std::shared_ptr<std::map<std::string, std::shared_ptr<Session>>> info::sessions = nullptr;

  void init_server_info(){
    info::locks = std::make_shared<std::map<std::string, std::shared_ptr<Lock>>>();
    info::sessions = std::make_shared<std::map<std::string, std::shared_ptr<Session>>>();

    // Only need to add self to memberlist/current node if we are a node (server)
    utility::parse::Address selfAddress = State::config->getAddress<app::Service::NODE>();
    auto iterator = State::memberList->find(selfAddress.toString());
    if (iterator == State::memberList->end()) {  // not found
      State::currentNode = std::make_shared<Node>(selfAddress);
      State::memberList->insert(std::make_pair(selfAddress.toString(), State::currentNode));
    } else {
      State::currentNode = iterator->second;
    }

    // Find leader
    if(State::config->flag.leader){
      // We are leader, used mainly for testing
      if(State::config->flag.debug){
        std::cout << termcolor::yellow << "We are leader" << termcolor::reset << std::endl;
      }
      State::master = std::make_shared<std::string>(selfAddress.toString());
    }else{
      // Run Paxos to find master
      State::master = std::make_shared<std::string>("");
    }
  }

  grpc::Status create_session(std::string client_id){
    // Check if we already have a session with the client
    if(info::sessions->find(client_id) != info::sessions->end()){
      if(State::config->flag.debug){
        std::cout << termcolor::yellow << "Client with id " << client_id << " already has a session that hasn't yet been terminated." << termcolor::reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client has an existing session.");
    }

    // Initialize session struct for this new session
    std::shared_ptr session = std::make_shared<Session>();
    session->client_id = client_id;
    session->start_time = chrono::system_clock::now();
    session->lease_length = chrono::milliseconds(utility::DEFAULT_LEASE_EXTENSION);
    session->block_reply = std::make_shared<msd::channel<int>>();
    session->locks = std::make_shared<std::map<std::string, std::shared_ptr<Lock>>>();
    session->terminated = false;
    session->terminate_indicator = std::make_shared<msd::channel<int>>();

    // Add session struct to our map of sessions
    info::sessions->insert(std::make_pair(client_id, session));

    // if(State::config->flag.debug){
    //   std::cout << termcolor::cyan << "Session created:" << endl << termcolor::grey
    //     << "client_id: " << session->client_id << endl
    //     << "start time: " << chrono::system_clock::to_time_t(session->start_time) << endl
    //     << "lease length: " << session->lease_length.count() << "ms" << termcolor::reset << endl;
    // }

    // Launch async function to loop and populate the channels for this session as needed
    // TODO: Make this a thread instead so we can use std::this_thread::sleep_for
    // and run the main session tracking logic once per second
    auto i = std::async(std::launch::async, maintain_session, info::sessions->at(client_id));

    return Status::OK;
  }

  void maintain_session(std::shared_ptr<Session> session){
    if(State::config->flag.debug){
      std::cout << termcolor::cyan << "Maintaining the following session:" << endl << termcolor::grey
        << "client_id: " << session->client_id << endl
        << "start time: " << chrono::system_clock::to_time_t(session->start_time) << endl
        << "lease length: " << session->lease_length.count() << "ms" << termcolor::reset << endl;
    }

  }

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
      grpc::Status status = node->endpoint.init_session(info::session_id);

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
