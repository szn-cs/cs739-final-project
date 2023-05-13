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
    grpc::Status close_session(ServerContext*, const interface::CloseSessionRequest*, interface::Empty*) override;
    grpc::Status keep_alive(ServerContext*, const interface::KeepAliveRequest*, interface::KeepAliveResponse*) override;
    grpc::Status open_lock(ServerContext*, const interface::OpenLockRequest*, interface::Empty*) override;
    grpc::Status delete_lock(ServerContext*, const interface::DeleteLockRequest*, interface::Empty*) override;
    grpc::Status acquire_lock(ServerContext*, const interface::AcquireLockRequest*, interface::AcquireLockResponse*) override;
    grpc::Status release_lock(ServerContext*, const interface::ReleaseLockRequest*, interface::Empty*) override;
    grpc::Status read(ServerContext*, const interface::ReadRequest*, interface::ReadResponse*) override;
    grpc::Status write(ServerContext*, const interface::WriteRequest*, interface::WriteResponse*) override;
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
    grpc::Status close_session(std::string);
    std::pair<grpc::Status, int64_t> keep_alive(std::string, chrono::system_clock::time_point);   // Used for communicating with a known master
    std::pair<grpc::Status, int64_t> keep_alive(std::string, std::map<std::string, LockStatus>);  // Used when in jeopardy
    grpc::Status open_lock(std::string, std::string);                                             // Used to create a new file (lock)
    grpc::Status delete_lock(std::string, std::string);                                           // Used to delete a file (lock)
    grpc::Status acquire_lock(std::string, std::string, LockStatus);
    grpc::Status release_lock(std::string, std::string);
    std::pair<grpc::Status, std::string> read(std::string, std::string);
    grpc::Status write(std::string, std::string, std::string);

    std::string address;  // <host:port>
    std::shared_ptr<interface::RPC::Stub> stub;
  };

}  // namespace rpc
