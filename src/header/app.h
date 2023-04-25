#pragma once
#include "./library.h"
#include "./rpc.h"
#include "./utility.h"

namespace app {

  struct Node {
    Node() = default;  // no default constructor
    Node(std::string address) : endpoint(rpc::Endpoint(address)){};
    Node(utility::parse::Address a) : Node(a.toString()){};

    rpc::Endpoint endpoint;
  };

  struct State {
    static std::shared_ptr<utility::parse::Config> config;
    static std::shared_ptr<std::map<std::string, std::shared_ptr<Node>>> memberList;  // addresses of nodes in cluster
    static std::shared_ptr<Node> currentNode;                                         // current machine's Node object
  };

  void initializeStaticInstance(std::shared_ptr<utility::parse::Config> config, std::vector<std::string> addressList);

}  // namespace app
