#pragma once

#include "./library.h"

namespace rpc {
  /**
    * Implementation of gRPC interface functionality  (which the server exposes through a specific port)
  */
  class RPC : public interface::RPC::Service {
   public:
    grpc::Status func(ServerContext*, const interface::Request*, interface::Response*) override;
    grpc::Status get_master(ServerContext*, const interface::Empty*, interface::GetMasterResponse*) override;
  };

  /**
     * Stores stub for RPC channel and may wrap RPC calls (acts as client) to the RPC endpoint (server)
     *
    */
  struct Endpoint {
    Endpoint() = default;  // empty instance
    Endpoint(std::string a) : address(a) {
      std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(a, grpc::InsecureChannelCredentials());
      stub = interface::RPC::NewStub(channel);
    }

    std::pair<grpc::Status, int> func(int v);
    std::pair<grpc::Status, std::string> get_master();

    std::string address;
    std::shared_ptr<interface::RPC::Stub> stub;
  };

}  // namespace rpc
