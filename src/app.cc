#include "common.h"

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

    fs::create_directories(fs::absolute(config->directory));  // create directory if doesn't exist

    State::memberList = std::make_shared<std::map<std::string, std::shared_ptr<Node>>>();

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

    if (app::State::config->flag.debug) {
      app::consensus::print_status();
      app::consensus::server_list();
    }
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

  grpc::Status close_session(std::string client_id) {
    if(info::sessions == nullptr) 
      throw "❌ info::sessions is null !";

    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Client with id " << client_id << " does not have a session to end." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Client has an existing session.");
    }

    if (info::sessions->at(client_id)->terminated) {
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
    if(session == nullptr) 
      throw "❌ session param is null !";
    session->terminated = true;

    // Release all locks
    for (auto it = session->locks->cbegin(); it != session->locks->cend();) {
      grpc::Status status = release_lock(session->client_id, it->first);
      if (!status.ok()) {
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
    if (info::sessions->find(client_id) == info::sessions->end()) {
      if (app::State::config->flag.debug) {
        std::cout << red << "No session established" << reset << std::endl;
      }
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

  int64_t handle_jeopardy(std::string client_id, google::protobuf::Map<std::string, LockStatus> locks) {
    // Create a session for the client
    grpc::Status sess_status = create_session(client_id);
    if (!sess_status.ok()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Unable to start new session with client with id " << client_id << "." << reset << std::endl;
      }
      return -1;
    }

    if (State::config->flag.debug) {
      std::cout << green << "Started new session with client with id " << client_id << "." << reset << std::endl;
    }

    // Try to acquire all locks previously owned by the client
    for (auto& [file_path, mode] : locks) {
      grpc::Status acquire_status = acquire_lock(client_id, file_path, mode);
      if (!acquire_status.ok()) {
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

    /* The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // IMPORTANT: The following code just checks if lock exists IN MEMORY (not needed if we check persistent)
    if (is_exists(file_path)) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " already exists." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Lock already exists.");
    }

    // if log exists, return OK

    // if log does not exist, create it within persistent memory
    if (State::config->flag.debug)
      cout << on_bright_cyan << "🧬 Trying to replicate command: "
           << "WRITE " << file_path << " " << reset << endl;
    std::string s = "";
    app::consensus::append_log(app::consensus::op_type::WRITE, file_path, s);

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

    /* The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    if (is_exists(file_path)) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " already exists." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "file doesn't exists.");
    }

    // if log does not exist, return an error
    // return grpc::Status(StatusCode::ABORTED, "The lock does not exist in the raft log.");

    // delete the log from raft storage
    fs::remove(file_path);

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
    if (!is_exists(file_path)) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " already exists." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "file doesn't exists.");
    }

    // if lock does not exist, return an error
    // return grpc::Status(StatusCode::ABORTED, "The lock does not exist in persistent storage.");

    // Check if lock exists in memory
    if (info::locks->find(file_path) == info::locks->end()) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " does not exist in memory." << reset << std::endl;
      }
      // Assume failure, copy struct from persistent into memory

      /* TODO:: The below stuff once we get raft configged correctly */

      std::shared_ptr<Lock> lock = std::make_shared<Lock>();
      lock->path = file_path;
      lock->owners = std::make_shared<std::map<std::string, bool>>();
      lock->status = LockStatus::FREE;
      lock->content = "";
      std::pair<std::string, std::shared_ptr<Lock>> entry = std::make_pair(file_path, lock);

      info::locks->insert(entry);
      info::sessions->at(client_id)->locks->insert(entry);
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

    /* The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // IMPORTANT: The following code just checks if lock exists IN MEMORY (not needed if we check persistent)
    if (!is_exists(file_path)) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " already exists." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Lock already exists.");
    }

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
    try {
      res.second = app::server::read_content(file_path);
    } catch (const std::exception& ex) {
      std::cerr << "Read error: " << ex.what() << std::endl;
    }

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

    /*  The below stuff once we get raft configged correctly */
    // Check if lock exists in persistent store
    // IMPORTANT: The following code just checks if lock exists IN MEMORY (not needed if we check persistent)
    if (!is_exists(file_path)) {
      if (State::config->flag.debug) {
        std::cout << yellow << "Lock with name " << file_path << " already exists." << reset << std::endl;
      }
      return grpc::Status(StatusCode::ABORTED, "Lock already exists.");
    }

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

    if (State::config->flag.debug)
      cout << on_bright_cyan << "🧬 Trying to replicate command: "
           << "WRITE " << file_path << " " << reset << endl;
    app::consensus::append_log(app::consensus::op_type::WRITE, file_path, content);

    return Status::OK;
  }

  std::string read_content(std::string path) {
    string base_directory = utility::concatenatePath(fs::absolute(app::State::config->directory), std::to_string(app::State::stuff.server_id_));
    string mapped_path = utility::concatenatePath(base_directory, path);

    ifstream f(mapped_path);  //taking file as inputstream
    string str;
    if (f) {
      ostringstream ss;
      ss << f.rdbuf();  // reading data
      str = ss.str();
    } else {
      throw("Failed to open file.");
    }
    return str;
  }

  bool is_exists(string path) {
    string base_directory = utility::concatenatePath(fs::absolute(app::State::config->directory), std::to_string(app::State::stuff.server_id_));
    string mapped_path = utility::concatenatePath(base_directory, path);

    return fs::exists(mapped_path);
  }

}  // namespace app::server
