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
}  // namespace rpc

namespace rpc {
  /************************ init_session rpcs ************************/
  Status RPC::init_session(ServerContext* context, const interface::InitSessionRequest* request, interface::Empty* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "RPC";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    if ((*(app::State::master)).compare(app::State::config->getAddress<app::Service::NODE>().toString()) == 0) {
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
}  // namespace rpc

namespace rpc {
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
} // namespace rpc

namespace rpc {
  /************************ keep_alive rpcs ************************/
  grpc::Status RPC::keep_alive(ServerContext* context, const interface::KeepAliveRequest* request, interface::KeepAliveResponse* response) {
    if (app::State::config->flag.debug) {
      const std::string className = "Endpoint";
      const string n = className + "::" + __func__;
      std::cout << grey << utility::getClockTime() << reset << yellow << n << reset << std::endl;
    }

    if ((*(app::State::master)).compare(app::State::config->getAddress<app::Service::NODE>().toString()) != 0) {
      // We are not master
      return Status(StatusCode::ABORTED, "This server is not master.");
    }

    if (context->IsCancelled()) {
      return Status(StatusCode::CANCELLED, "Exceeded deadline.");
    }

    int32_t lease_duration = app::server::attempt_extend_session(request->client_id());
    if(lease_duration < 0){
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

}  // namespace rpc

namespace rpc {
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

}  // namespace rpc

namespace rpc {
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
}  // namespace rpc

namespace rpc {
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
}  // namespace rpc

namespace rpc {
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
}  // namespace rpc

namespace rpc {
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
}  // namespace rpc

namespace rpc {
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

/**
 * @brief Setup/initialization for a server
 *
 */
namespace app {

  std::shared_ptr<std::map<std::string, std::shared_ptr<Node>>> State::memberList = nullptr;
  std::shared_ptr<utility::parse::Config> State::config = nullptr;
  std::shared_ptr<Node> State::currentNode = nullptr;
  std::shared_ptr<std::string> State::master = nullptr;
  app::consensus::server_stuff State::stuff = app::consensus::server_stuff();

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
      std::cout << grey << "Size of cluster: " << State::memberList->size() << reset << std::endl;
      cout << grey << "Using config file at: " << config->config << reset << endl;
    }

    {  // initialize static/object datastructures
      using namespace app::consensus;

      app::State::stuff.server_id_ = config->consensus.serverId;
      app::State::stuff.port_ = config->consensus.port;
      app::State::stuff.addr_ = config->consensus.address;
      app::State::stuff.endpoint_ = app::State::stuff.addr_ + ":" + std::to_string(app::State::stuff.port_);

      if (config->consensus.asyncSnapshotCreation) {
        CALL_TYPE = raft_params::async_handler;
      } else if (config->consensus.asyncHandler) {
        ASYNC_SNAPSHOT_CREATION = true;
      }
    }
  }

  void init_consensus() {
    using namespace app::consensus;

    if (app::State::config->flag.debug) {
      std::cout << cyan << "    Server ID:    " << app::State::stuff.server_id_ << std::endl;
      std::cout << "    Endpoint:     " << app::State::stuff.endpoint_ << std::endl;
      if (CALL_TYPE == raft_params::async_handler)
        std::cout << "    async handler is enabled" << std::endl;
      if (ASYNC_SNAPSHOT_CREATION)
        std::cout << "    snapshots are created asynchronously" << reset << std::endl;
    }

    init_raft(cs_new<consensus_state_machine>(ASYNC_SNAPSHOT_CREATION), "./tmp/");
  }

}  // namespace app

namespace app::server {
  std::shared_ptr<std::map<std::string, std::shared_ptr<Lock>>> info::locks = nullptr;
  std::shared_ptr<std::map<std::string, std::shared_ptr<Session>>> info::sessions = nullptr;
  // std::shared_ptr<std::map<std::string, std::thread>> info::session_managers = nullptr;

  void init_server_info() {
    info::locks = std::make_shared<std::map<std::string, std::shared_ptr<Lock>>>();
    info::sessions = std::make_shared<std::map<std::string, std::shared_ptr<Session>>>();
    // info::session_managers = std::make_shared<std::map<std::string, std::thread>>();

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
    if (State::config->flag.leader) {
      // We are leader, used mainly for testing
      if (State::config->flag.debug) {
        std::cout << yellow << "We are leader" << reset << std::endl;
      }
      State::master = std::make_shared<std::string>(selfAddress.toString());
    } else {
      // Run Paxos to find master
      State::master = std::make_shared<std::string>("");
    }
  }

  grpc::Status create_session(std::string client_id) {
    // Check if we already have a session with the client
    if (info::sessions->find(client_id) != info::sessions->end()) {
      if (!info::sessions->at(client_id)->terminated) {
        if (State::config->flag.debug) {
          std::cout << yellow << "Client with id " << client_id << " already has a session that hasn't yet been terminated." << reset << std::endl;
        }
        return grpc::Status(StatusCode::ABORTED, "Client has an existing session.");
      }
    }

    // Initialize session struct for this new session
    std::shared_ptr session = std::make_shared<Session>();
    session->client_id = client_id;
    session->start_time = chrono::system_clock::now();
    session->lease_length = chrono::milliseconds(utility::DEFAULT_LEASE_EXTENSION);
    session->block_reply = std::make_shared<msd::channel<int>>();
    session->locks = std::make_shared<std::map<std::string, std::shared_ptr<Lock>>>();
    session->terminated = false;

    // Add session struct to our map of sessions
    info::sessions->insert(std::make_pair(client_id, session));

    // Launch a thread to loop and populate the channels for this session as needed
    std::thread t(maintain_session, info::sessions->at(client_id));
    t.detach();

    return Status::OK;
  }

  grpc::Status close_session(std::string client_id){
    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session to end." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client has an existing session.");
    }

    if(info::sessions->at(client_id)->terminated){
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session to end." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client has an existing session.");
    }

    end_session(info::sessions->at(client_id));
    return Status::OK;
  }

  void maintain_session(std::shared_ptr<Session> session) {
    if (State::config->flag.debug) {
      std::cout << cyan << "Maintaining the following session:" << endl
                << grey << "client_id: " << session->client_id << endl
                << "start time: " << chrono::system_clock::to_time_t(session->start_time) << endl
                << "lease length: " << session->lease_length.count() << "ms" << reset << endl;
    }

    // We don't want to block the first keep alive
    int in = 1;
    in >> *(session->block_reply);

    while (true) {
      chrono::system_clock::time_point lease_expires = session->start_time + session->lease_length;
      chrono::system_clock::duration time_until_expire = chrono::nanoseconds(0);

      // Check if we are now within a second of expiration
      if (lease_expires > chrono::system_clock::now()) {
        time_until_expire = lease_expires - std::chrono::system_clock::now();
      }

      // Session timed out
      if (time_until_expire == chrono::nanoseconds(0)) {
        if (State::config->flag.debug) {
          std::cout << yellow << "Lease with client " << session->client_id << " is expired (i.e. session maitenence thread dies)." << reset << std::endl;
        }

        end_session(session);
        in >> *(session->block_reply);
        return;
      }

      // Ready to reply to client
      if (time_until_expire <= chrono::seconds(1)) {
        if (State::config->flag.debug) {
          std::cout << grey << "Triggering keep_alive response to " << session->client_id << "." << reset << std::endl;
        }
        in >> *(session->block_reply);
      }
      // Run the loop every second, a granularity that should suffice
      std::this_thread::sleep_for(chrono::seconds(1));
    }
  }

  void end_session(std::shared_ptr<Session> session) {
    session->terminated = true;

    // Release all locks
    for (auto it = session->locks->cbegin(); it != session->locks->cend();) {
      grpc::Status status = release_lock(session->client_id, it->first);
      if(!status.ok()){
        if (State::config->flag.debug) {
          std::cout << grey << "Error releasing lock " << it->first << " by client " << session->client_id << "." << reset << std::endl;
        }
      }
      it = session->locks->cbegin();
    }
    if (State::config->flag.debug) {
      std::cout << cyan << "Terminated session." << reset << std::endl;
    }
  }

  int64_t attempt_extend_session(std::string client_id) {
    // Check if client has a session, if not this is likely a jeopardy RPC
    if(info::sessions->find(client_id) == info::sessions->end()){
      return -1;
    }
    std::shared_ptr<Session> session = info::sessions->at(client_id);

    // Will block until the session manager indicates it is time to send a reply
    int i = 0;
    i << *(session->block_reply);

    // Set the correct lease length
    if (!session->terminated) {
      if (State::config->flag.debug) {
        std::cout << grey << "Extending lease for client " << session->client_id << "." << reset << std::endl;
      }
      session->lease_length = session->lease_length + chrono::milliseconds(utility::DEFAULT_LEASE_EXTENSION);
    }

    // Return so that we can send response from the rpc service
    return session->lease_length.count();
  }

  int64_t handle_jeopardy(std::string client_id, google::protobuf::Map<std::string, LockStatus> locks){
    // Create a session for the client
    grpc::Status sess_status = create_session(client_id);
    if(!sess_status.ok()){
      if (State::config->flag.debug) {
        std::cout << yellow << "Unable to start new session with client with id " << client_id << "." << reset << std::endl;
      }
      return -1;
    }

    if (State::config->flag.debug) {
      std::cout << green << "Started new session with client with id " << client_id << "." << reset << std::endl;
    }

    // Try to acquire all locks previously owned by the client
    for(auto & [file_path, mode] : locks){
      grpc::Status acquire_status = acquire_lock(client_id, file_path, mode);
      if(!acquire_status.ok()){
        // If we are unable to acquire any of the locks previously held by the client, end session
        if (State::config->flag.debug) {
          std::cout << yellow << "Client with id " << client_id << " unable to acquire lock " << file_path << "." << reset << std::endl;
        }
        end_session(info::sessions->at(client_id));
      }
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " acquired lock " << file_path << " successfully." << reset << std::endl;
      }
    }

    // Extend the client's new session, similar to how they would send keep_alive right after session creation
    return attempt_extend_session(client_id);
  }

  grpc::Status open_lock(std::string client_id, std::string file_path) {
    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session established with this server." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client does not have an existing session.");
    }

    /* TODO:: The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // raft.get_log(file_path) ??

    // IMPORTANT: The following code just checks if lock exists IN MEMORY (not needed if we check persistent)
    if (info::locks->find(file_path) != info::locks->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " already exists." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Lock already exists.");
    }

    // if log exists, return OK

    // if log does not exist, create it within persistent memory
    // raft.set(file_path, "");

    // Add lock to in memory data structures
    std::shared_ptr<Lock> lock = std::make_shared<Lock>();
    lock->path = file_path;
    lock->content = "";
    lock->owners = std::make_shared<std::map<std::string, bool>>();
    lock->status = LockStatus::FREE;
    std::pair<std::string, std::shared_ptr<Lock>> entry = std::make_pair(file_path, lock);

    info::locks->insert(entry);
    info::sessions->at(client_id)->locks->insert(entry);

    return Status::OK;
  }

  grpc::Status delete_lock(std::string client_id, std::string file_path) {
    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session established with this server." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client does not have an existing session.");
    }

    // Check if this client is holding the lock for this file
    std::shared_ptr<Session> session = info::sessions->at(client_id);
    if (session->locks->find(file_path) == session->locks->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a lock under path " << file_path << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client does not hold this lock.");
    }

    // Ensure the client is holding the lock in exclusive mode
    if (session->locks->at(file_path)->status != LockStatus::EXCLUSIVE) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a lock under path " << file_path << " in exclusive mode." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client does not hold this lock in exclusive mode.");
    }

    /* TODO:: The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // raft.get_log(file_path) ??

    // if log does not exist, return an error
    // return grpc::Status(StatusCode::ABORTED, "The lock does not exist in the raft log.");

    // delete the log from raft storage
    // raft.delete(file_path) ??

    // Delete the lock from in memory storage
    session->locks->erase(file_path);
    info::locks->erase(file_path);

    return Status::OK;
  }

  grpc::Status acquire_lock(std::string client_id, std::string file_path, LockStatus mode) {
    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session established with this server." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client does not have an existing session.");
    }
    std::shared_ptr<Session> session = info::sessions->at(client_id);

    if (mode != LockStatus::EXCLUSIVE && mode != LockStatus::SHARED) {
      return grpc::Status(StatusCode::ABORTED, "Can't request a lock in free mode.");
    }

    /* TODO:: The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // raft.get_log(file_path) ??

    // if lock does not exist, return an error
    // return grpc::Status(StatusCode::ABORTED, "The lock does not exist in persistent storage.");

    // Check if lock exists in memory
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " does not exist in memory." << reset << std::endl;
      }
      // Assume failure, copy struct from persistent into memory

      /* TODO:: The below stuff once we get raft configged correctly */
      /*
          auto log = raft.get_log(file_path); // NEED RAFT FOR THIS
          std::shared_ptr<Lock> lock = std::make_shared<Lock>();
          lock->path = log.file_path;
          lock->owners = std::make_shared<std::map<std::string, bool>>();
          lock->status = LockStatus::FREE;
          lock->content = "";
          std::pair<std::string, std::shared_ptr<Lock>> entry = std::make_pair(file_path, lock);

          info::locks->insert(entry);
          info::sessions->at(client_id)->locks->insert(entry);
      */
      return grpc::Status(StatusCode::ABORTED, "Lock does not exist in memory.");  // Get rid once we implement above
    }

    // Get the lock and check it's current status
    std::shared_ptr<app::server::Lock> l = info::locks->at(file_path);
    std::map<std::string, bool>::iterator is_owner = l->owners->find(client_id);

    if (l->status == LockStatus::EXCLUSIVE) {
      // Let client know someone already has the lock
      return grpc::Status(StatusCode::ABORTED, "Someone already holds the lock in exclusive mode");

    } else if (l->status == LockStatus::SHARED) {
      // If client tries to acquire the lock in shared mode, succeed
      if (mode == LockStatus::SHARED) {
        if (is_owner == l->owners->end()) {
          l->owners->insert(std::make_pair(client_id, true));
        } else {
          is_owner->second = true;
        }

        // Copy updated lock into this client's session data
        std::map<std::string, std::shared_ptr<Lock>>::iterator it = session->locks->find(file_path);
        if (it == session->locks->end()) {
          session->locks->insert(std::make_pair(file_path, l));
        } else {
          it->second = l;
        }
        return Status::OK;
      } else {
        // Fails to open lock in exclusive mode, since lock is owned in shared mode
        return grpc::Status(StatusCode::ABORTED, "Someone already holds the lock, can't claim exclusively.");
      }
    } else {
      // Success, client claims lock
      if (is_owner == l->owners->end()) {
        l->owners->insert(std::make_pair(client_id, true));
      } else {
        is_owner->second = true;
      }
      l->status = mode;

      // Update lock in session and lock data
      std::map<std::string, std::shared_ptr<Lock>>::iterator it = session->locks->find(file_path);
      if (it == session->locks->end()) {
        session->locks->insert(std::make_pair(file_path, l));
      } else {
        it->second = l;
      }

      info::locks->find(file_path)->second = l;

      return Status::OK;
    }
    return Status::CANCELLED;  // Never will make it here
  }

  grpc::Status release_lock(std::string client_id, std::string file_path) {
    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session established with this server." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client does not have an existing session.");
    }
    std::shared_ptr<Session> session = info::sessions->at(client_id);


    /* TODO:: The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // raft.get_log(file_path) ??

    // if lock does not exist, return an error
    // return grpc::Status(StatusCode::ABORTED, "The lock does not exist in persistent storage.");

    // Grab lock from session locks
    std::map<std::string, std::shared_ptr<Lock>>::iterator it = session->locks->find(file_path);

    if (it == session->locks->end() || it->second == nullptr) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " is not in this session's locks." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Lock was not found among this session's locks.");
    }
    std::shared_ptr<Lock> l = it->second;

    // Check if client is an owner
    std::map<std::string, bool>::iterator is_owner = l->owners->find(client_id);
    if (is_owner == l->owners->end() || !is_owner->second) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " is not owned by this client." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "You were not recorded as an owner of this lock.");
    }

    // The lock exists, client is owner
    if (l->status == LockStatus::EXCLUSIVE) {
      // Remove client from owners list, free the lock, delete from session locks
      l->owners->erase(client_id);
      l->status = LockStatus::FREE;
      session->locks->erase(file_path);

      // Set this new lock configuration in our server-wide lock map
      info::locks->find(file_path)->second = l;

      return Status::OK;
    } else {
      // Remove client from lock owners, set mode if 0 owners, delete from session locks
      l->owners->erase(client_id);
      if (l->owners->size() == 0) {
        l->status = LockStatus::FREE;
      }
      session->locks->erase(file_path);

      // Set new lock configuration in server-wide lock map
      info::locks->find(file_path)->second = l;
      return Status::OK;
    }
  }

  std::pair<grpc::Status, std::string> read(std::string client_id, std::string file_path) {
    // To store return values
    std::pair<grpc::Status, std::string> res = std::make_pair(Status::OK, "");

    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session established with this server." << reset << std::endl;
      }
      res.first = grpc::Status(StatusCode::ABORTED, "Client does not have an existing session.");
      return res;
    }
    std::shared_ptr<Session> session = info::sessions->at(client_id);

    /* TODO:: The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // raft.get_log(file_path) ??

    // if lock does not exist, return an error
    // return grpc::Status(StatusCode::ABORTED, "The lock does not exist in persistent storage.");

    // Grab lock from session locks
    std::map<std::string, std::shared_ptr<Lock>>::iterator it = session->locks->find(file_path);

    if (it == session->locks->end() || it->second == nullptr) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " is not in this session's locks." << reset << std::endl;
      }
      res.first = grpc::Status(StatusCode::ABORTED, "Lock was not found among this session's locks.");
      return res;
    }
    std::shared_ptr<Lock> l = it->second;

    // Check if client is an owner
    std::map<std::string, bool>::iterator is_owner = l->owners->find(client_id);
    if (is_owner == l->owners->end() || !is_owner->second) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " is not owned by this client." << reset << std::endl;
      }
      res.first = grpc::Status(StatusCode::ABORTED, "You were not recorded as an owner of this lock.");
      return res;
    }

    // TODO: The "hello" string below would hold the persistent lock's content
    res.second = "hello";  // raft.get(file_path)
    return res;
  }

  grpc::Status write(std::string client_id, std::string file_path, std::string content) {
    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session established with this server." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client does not have an existing session.");
    }
    std::shared_ptr<Session> session = info::sessions->at(client_id);

    /* TODO:: The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // raft.get_log(file_path) ??

    // if lock does not exist, return an error
    // return grpc::Status(StatusCode::ABORTED, "The lock does not exist in persistent storage.");

    // Grab lock from session locks
    std::map<std::string, std::shared_ptr<Lock>>::iterator it = session->locks->find(file_path);

    if (it == session->locks->end() || it->second == nullptr) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " is not in this session's locks." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Lock was not found among this session's locks.");
    }
    std::shared_ptr<Lock> l = it->second;

    // Check if client is an owner
    std::map<std::string, bool>::iterator is_owner = l->owners->find(client_id);
    if (is_owner == l->owners->end() || !is_owner->second) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " is not owned by this client." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "You were not recorded as an owner of this lock.");
    }

    // TODO: Set the log entry for the filepath
    // raft.set(file_path, content);

    return Status::OK;
  }

}  // namespace app::server

namespace app::client {
  std::string info::session_id;
  chrono::system_clock::time_point info::lease_start;
  chrono::milliseconds info::lease_length;
  std::shared_ptr<map<std::string, LockStatus>> info::locks;
  bool info::jeopardy;
  bool info::expired;
  std::shared_ptr<Node> info::master;

  grpc::Status start_session() {
    using namespace std::chrono;

    // Session ID is simply the client's ip:port, acting as a simple uniquifier
    info::session_id = app::State::config->getAddress<app::Service::NODE>().toString();
    info::locks = std::make_shared<std::map<std::string, LockStatus>>();
    info::jeopardy = false;
    info::expired = false;
    info::lease_length = milliseconds(utility::DEFAULT_LEASE_DURATION);
    info::master = nullptr;

    // Lease start is in seconds since epoch
    info::lease_start = system_clock::now();

    // Try to establish session with all nodes in cluster
    for (const auto& [key, node] : *(State::memberList)) {
      grpc::Status status = node->endpoint.init_session(info::session_id);

      if (!status.ok()) {  // If server is down or replies with grpc::StatusCode::ABORTED
        if (State::config->flag.debug) {
          cout << grey << "Node " << key << " replied with an error." << reset << endl;
        }
        continue;
      }

      // Session established with server
      info::master = node;
      break;
    }

    if (info::master == nullptr) {
      if (State::config->flag.debug) {
        cout << red << "Could not establish a session with any server." << reset << endl;
      }
      return Status::CANCELLED;
    }

    // Thread to handle maintaining the session (i.e. issuing keep_alives and such)
    std::thread t(maintain_session);
    t.detach();

    return Status::OK;
  }

  void close_session(){
    if (State::config->flag.debug) {
      cout << yellow << "Ending session." << reset << endl;
    }

    if(info::session_id.empty()){
      if (State::config->flag.debug) {
        cout << red << "No session to end." << reset << endl;
      }
      return;
    }

    grpc::Status status = info::master->endpoint.close_session(info::session_id);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully closed session." << reset << endl;
      }
    } else {
      if (State::config->flag.debug) {
        cout << red << "Failed to close session." << reset << endl;
      }
    }
  }

  void maintain_session() {
    if (State::config->flag.debug) {
      cout << yellow << "Beginning session maitenence." << reset << endl;
    }

    // Shouldn't ever happen
    if (info::master == nullptr) {
      return;
    }

    auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);

    while (true) {
      // Wait until response or the deadline is reached
      chrono::system_clock::time_point deadline = info::lease_start + info::lease_length;
      std::pair<grpc::Status, int64_t> r = info::master->endpoint.keep_alive(info::session_id, deadline);
      auto [status, new_lease_length] = r;

      // If we successfully heard back from server before deadline
      if (status.ok()) {
        if (State::config->flag.debug) {
          cout << grey << "keep_alive response received. Lease extended." << reset << endl;
        }
        info::lease_length = chrono::milliseconds(new_lease_length);
      } else {
        if (State::config->flag.debug) {
          cout << red << "Entering jeopardy." << reset << endl;
        }
        info::jeopardy = true;

        // Send keep alives which include the keys we believe we own to every server, keep doing so
        // until we hit jeopardy duration
        bool looping = true;
        while(looping){
          for (const auto& [key, node] : *(State::memberList)) {
            std::pair<grpc::Status, int64_t> res = node->endpoint.keep_alive(info::session_id, *(info::locks));

            if (!res.first.ok()) {  // If server is down or replies with grpc::StatusCode::ABORTED
              if (State::config->flag.debug) {
                cout << grey << "Node " << key << " replied with an error." << reset << endl;
              }
              continue;
            }

            // Master was found, check if session is still valid
            if(res.second < 0){
              // Indication that lease is expired, master didn't extend it
              info::expired = true;
              return;
            }

            // Update session info
            info::master = node;
            info::lease_start = chrono::system_clock::now();
            info::lease_length = chrono::milliseconds(res.second);
            info::jeopardy = false;

            // Stop looping
            looping = false;
            break;
          }

          // See if we have exceeded jeopardy duration
          if(chrono::system_clock::now() > stopJeopardy){
            info::expired = true;
            return;
          }
        }
        return;
      }
    }
  }

  bool open_lock(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if(info::jeopardy || info::expired){
        cout << grey << "Session expired." << reset << endl;
        return false;
      }
    }

    grpc::Status status = info::master->endpoint.open_lock(info::session_id, file_path);

    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully opened lock." << reset << endl;
      }
      return true;
    } else {
      if (State::config->flag.debug) {
        cout << red << "Failed to open lock." << reset << endl;
      }
      return false;
    }
  }

  bool delete_lock(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if(info::jeopardy || info::expired){
        cout << grey << "Session expired." << reset << endl;
        return false;
      }
    }

    grpc::Status status = info::master->endpoint.delete_lock(info::session_id, file_path);

    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully deleted lock." << reset << endl;
      }
      return true;
    } else {
      if (State::config->flag.debug) {
        cout << red << "Failed to delete lock." << reset << endl;
      }
      return false;
    }
  }

  grpc::Status acquire_lock(std::string file_path, LockStatus mode) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if(info::jeopardy || info::expired){
        cout << grey << "Session expired." << reset << endl;
        return grpc::Status(StatusCode::ABORTED, "Session expired.");
      }
    }

    grpc::Status status = info::master->endpoint.acquire_lock(info::session_id, file_path, mode);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully acquired lock." << reset << endl;
      }
      info::locks->insert(std::make_pair(file_path, mode));
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to acquire lock." << reset << endl;
      }
    }

    return status;
  }

  grpc::Status release_lock(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if(info::jeopardy || info::expired){
        cout << grey << "Session expired." << reset << endl;
        return grpc::Status(StatusCode::ABORTED, "Session expired.");
      }
    }

    // Check if we hold the lock client side
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        cout << red << "We don't own lock we are trying to release." << reset << endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client doesn't have the lock you are trying to release.");
    }

    grpc::Status status = info::master->endpoint.release_lock(info::session_id, file_path);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully released lock." << reset << endl;
      }
      info::locks->erase(file_path);
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to release lock." << reset << endl;
      }
    }

    return status;
  }

  std::pair<grpc::Status, std::string> read(std::string file_path) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if(info::jeopardy || info::expired){
        cout << grey << "Session expired." << reset << endl;
        
        grpc::Status s = grpc::Status(StatusCode::ABORTED, "Session expired.");
        std::pair<grpc::Status, std::string> res = std::make_pair(s, "");
        return res;
      }
    }

    // Check if we hold the lock client side
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        cout << red << "We don't own lock we are trying to read from." << reset << endl;
      }
      grpc::Status s = grpc::Status(StatusCode::ABORTED, "Client doesn't have the lock you are trying to read from.");
      std::pair<grpc::Status, std::string> res = std::make_pair(s, "");

      return res;
    }

    std::pair<grpc::Status, std::string> res = info::master->endpoint.read(info::session_id, file_path);

    // Debugging
    if (res.first.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully read from file." << reset << endl;
      }
      info::locks->erase(file_path);
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to read from file." << reset << endl;
      }
    }

    return res;
  }

  grpc::Status write(std::string file_path, std::string content) {
    if (info::jeopardy) {
      // Here we would wait for either timeout, or for the session to be reestablished
      if (State::config->flag.debug) {
        cout << grey << "We are in jeopardy." << reset << endl;
      }
      auto stopJeopardy = info::lease_start + info::lease_length + chrono::milliseconds(utility::JEAPARDY_DURATION);
      std::this_thread::sleep_until(stopJeopardy);
      if(info::jeopardy || info::expired){
        cout << grey << "Session expired." << reset << endl;
        return grpc::Status(StatusCode::ABORTED, "Session expired.");
      }
    }

    // Check if we hold the lock client side
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        cout << red << "We don't own lock we are trying to write to." << reset << endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client doesn't have the lock you are trying to write to.");
    }

    grpc::Status status = info::master->endpoint.write(info::session_id, file_path, content);

    // Debugging
    if (status.ok()) {
      if (State::config->flag.debug) {
        cout << green << "Successfully write to file." << reset << endl;
      }
    } else {
      if (State::config->flag.debug) {
        cout << red << "Client api unable to write to file." << reset << endl;
      }
    }

    return status;
  }

}  // namespace app::client
