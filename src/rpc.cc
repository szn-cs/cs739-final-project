#include "common.h"

namespace rpc {

  /************************ ping rpcs TODO: These aren't needed ************************/
  Status RPC::ping(ServerContext* context, const interface::Empty* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "RPC";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    return Status::OK;
  }

  grpc::Status Endpoint::ping() {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    Empty request;
    Empty response;

    if (app::State::config->flag.debug) cout << yellow << "calling RPC::func @ " << this->address << " endpoint;" << reset << endl;
    grpc::Status status = this->stub->ping(&context, request, &response);

    return status;
  }

  /************************ init_session rpcs ************************/
  Status RPC::init_session(ServerContext* context, const interface::InitSessionRequest* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "RPC";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    // if ((*(app::State::master)).compare(app::State::config->getAddress<app::Service::NODE>().toString()) == 0) {
    if (app::State::stuff.server_id_ == app::State::stuff.raft_instance_->get_leader()) {
      cout << yellow << "We are leader" << reset << endl;
      return app::server::create_session(request->client_id());
    }
    return Status::CANCELLED;
  }

  grpc::Status Endpoint::init_session(std::string client_id) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    InitSessionRequest request;
    Empty response;

    request.set_client_id(client_id);

    grpc::Status status = this->stub->init_session(&context, request, &response);

    return status;
  }

  /************************ close_session rpcs ************************/
  grpc::Status RPC::close_session(ServerContext* context, const interface::CloseSessionRequest* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    return app::server::close_session(request->client_id());
  }

  grpc::Status Endpoint::close_session(std::string client_id) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    CloseSessionRequest request;
    Empty response;

    request.set_client_id(client_id);

    grpc::Status status = this->stub->close_session(&context, request, &response);

    return status;
  }

  /************************ keep_alive rpcs ************************/
  grpc::Status RPC::keep_alive(ServerContext* context, const interface::KeepAliveRequest* request, interface::KeepAliveResponse* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    // if ((*(app::State::master)).compare(app::State::config->getAddress<app::Service::NODE>().toString()) != 0) {
    if (app::State::stuff.server_id_ != app::State::stuff.raft_instance_->get_leader()) {
      // We are not master
      return Status(StatusCode::ABORTED, "This server is not master.");
    }

    if (context->IsCancelled()) {
      if (app::State::config->flag.debug) {
        std::cout << red << "RPC canceled master" << reset << std::endl;
      }
      return Status(StatusCode::CANCELLED, "Exceeded deadline.");
    }

    int32_t lease_duration = app::server::attempt_extend_session(request->client_id());
    if (lease_duration < 0) {
      // We return -1 if there was no session to extend
      lease_duration = app::server::handle_jeopardy(request->client_id(), request->locks());
    }
    response->set_lease_duration(lease_duration);

    return Status::OK;
  }

  std::pair<grpc::Status, int64_t> Endpoint::keep_alive(std::string client_id, chrono::system_clock::time_point deadline) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    KeepAliveRequest request;
    KeepAliveResponse response;
    request.set_client_id(client_id);
    context.set_deadline(deadline);

    grpc::Status status = this->stub->keep_alive(&context, request, &response);

    std::pair<grpc::Status, int64_t> res;
    res.first = status;
    res.second = response.lease_duration();

    return res;
  }

  std::pair<grpc::Status, int64_t> Endpoint::keep_alive(std::string client_id, std::map<std::string, LockStatus> locks) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    KeepAliveRequest request;
    KeepAliveResponse response;
    request.set_client_id(client_id);
    *(request.mutable_locks()) = google::protobuf::Map<std::string, LockStatus>(locks.begin(), locks.end());

    grpc::Status status = this->stub->keep_alive(&context, request, &response);

    std::pair<grpc::Status, int64_t> res;
    res.first = status;
    res.second = response.lease_duration();
    return res;
  }

  /************************ open_lock rpcs ************************/
  grpc::Status RPC::open_lock(ServerContext* context, const interface::OpenLockRequest* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    return app::server::open_lock(request->client_id(), request->file_path());
  }

  grpc::Status Endpoint::open_lock(std::string client_id, std::string file_path) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    OpenLockRequest request;
    Empty response;
    request.set_client_id(client_id);
    request.set_file_path(file_path);

    grpc::Status status = this->stub->open_lock(&context, request, &response);

    return status;
  }

  /************************ close_lock rpcs ************************/
  grpc::Status RPC::delete_lock(ServerContext* context, const interface::DeleteLockRequest* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    return app::server::delete_lock(request->client_id(), request->file_path());
  }

  grpc::Status Endpoint::delete_lock(std::string client_id, std::string file_path) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    DeleteLockRequest request;
    Empty response;
    request.set_client_id(client_id);
    request.set_file_path(file_path);

    grpc::Status status = this->stub->delete_lock(&context, request, &response);

    return status;
  }

  /************************ acquire_lock rpcs ************************/
  grpc::Status RPC::acquire_lock(ServerContext* context, const interface::AcquireLockRequest* request, interface::AcquireLockResponse* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }
    return app::server::acquire_lock(request->client_id(), request->file_path(), request->mode());
  }

  grpc::Status Endpoint::acquire_lock(std::string client_id, std::string file_path, LockStatus mode) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    AcquireLockRequest request;
    AcquireLockResponse response;
    request.set_client_id(client_id);
    request.set_file_path(file_path);
    request.set_mode(mode);

    grpc::Status status = this->stub->acquire_lock(&context, request, &response);

    return status;
  }

  /************************ release_lock rpcs ************************/
  grpc::Status RPC::release_lock(ServerContext* context, const interface::ReleaseLockRequest* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    return app::server::release_lock(request->client_id(), request->file_path());
  }

  grpc::Status Endpoint::release_lock(std::string client_id, std::string file_path) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    ReleaseLockRequest request;
    Empty response;
    request.set_client_id(client_id);
    request.set_file_path(file_path);

    grpc::Status status = this->stub->release_lock(&context, request, &response);

    return status;
  }

  /************************ read rpcs ************************/
  grpc::Status RPC::read(ServerContext* context, const interface::ReadRequest* request, interface::ReadResponse* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }
    std::pair<grpc::Status, std::string> res = app::server::read(request->client_id(), request->file_path());
    response->set_content(res.second);
    return res.first;
  }

  std::pair<grpc::Status, std::string> Endpoint::read(std::string client_id, std::string file_path) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    ReadRequest request;
    ReadResponse response;
    request.set_client_id(client_id);
    request.set_file_path(file_path);

    grpc::Status status = this->stub->read(&context, request, &response);
    std::pair<grpc::Status, std::string> res = std::make_pair(status, response.content());

    return res;
  }

  /************************ write rpcs ************************/
  grpc::Status RPC::write(ServerContext* context, const interface::WriteRequest* request, interface::WriteResponse* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    return app::server::write(request->client_id(), request->file_path(), request->content());
  }

  grpc::Status Endpoint::write(std::string client_id, std::string file_path, std::string content) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    ClientContext context;
    WriteRequest request;
    WriteResponse response;
    request.set_client_id(client_id);
    request.set_file_path(file_path);
    request.set_content(content);

    grpc::Status status = this->stub->write(&context, request, &response);

    return status;
  }
}  // namespace rpc
