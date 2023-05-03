#include "./header/common.h"

namespace rpc {

  Status RPC::func(ServerContext* context, const interface::Request* request, interface::Response* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "RPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    response->set_value(request->value());

    return Status::OK;
  }

  // calls into the RPC::func() implementation without exposing grpc complexity.
  std::pair<grpc::Status, int> Endpoint::func(int v) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    Request request;
    Response response;

    request.set_value(v);

    if (app::State::config->flag.debug) cout << yellow << "calling RPC::func @ " << this->address << " endpoint;" << reset << endl;
    grpc::Status status = this->stub->func(&context, request, &response);

    return std::make_pair(status, response.value());
  }


  Status RPC::get_master(ServerContext* context, const interface::Empty* request, interface::GetMasterResponse* response){
    if (app::State::config->flag.debug) {
      const std::string className = "RPC";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    response->set_addr(*(app::State::master));
    return Status::OK;
  }

  std::pair<grpc::Status, std::string> Endpoint::get_master() {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << termcolor::grey << utility::getClockTime() << termcolor::reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    Empty request;
    GetMasterResponse response;


    if (app::State::config->flag.debug) cout << yellow << "calling RPC::func @ " << this->address << " endpoint;" << reset << endl;
    grpc::Status status = this->stub->get_master(&context, request, &response);

    return std::make_pair(status, response.addr());
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
    }
  }

}  // namespace app

/**
 * @brief 
 * 
 */
namespace app {




} // namespace app


namespace client {
  uint64_t client::info::session_id;

  void init_client(){
    
  }

  void start_session(){

  }
}
