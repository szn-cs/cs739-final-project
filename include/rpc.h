#pragma once

#include "library.h"

namespace rpc {
  /**
    * Implementation of gRPC interface functionality  (which the server exposes through a specific port)
  */
  class RPC : public interface::RPC::Service {
   public:
    grpc::Status ping(ServerContext*, const interface::Empty*, interface::Empty*) override;
    grpc::Status init_session(ServerContext*, const interface::InitSessionRequest*, interface::Empty*) override;
    grpc::Status keep_alive(ServerContext*, const interface::KeepAliveRequest*, interface::KeepAliveResponse*) override;
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

    grpc::Status ping();
    grpc::Status init_session(std::string);
    std::pair<grpc::Status, int64_t> keep_alive(std::string, chrono::system_clock::time_point);                                     // Used for communicating with a known master
    std::pair<grpc::Status, int64_t> keep_alive(std::string, std::map<std::string, LockStatus>, chrono::system_clock::time_point);  // Used when in jeopardy

    std::string address;
    std::shared_ptr<interface::RPC::Stub> stub;
  };

}  // namespace rpc