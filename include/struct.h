// Definitions of shared structures - prevent circular dependency & forward declaration mess.
#pragma once

#include "library.h"
#include "utility.h"

// maximum file path supported by our service
#define MAX_PATH_SIZE 256

namespace app::consensus {
  using namespace nuraft;

  enum class op_type : int {
    CREATE = 0x2,
    WRITE = 0x0,
    DELETE = 0x1,
  };

  typedef struct server_stuff {
    server_stuff()
        : server_id_(1), addr_("localhost"), port_(25000), raft_logger_(nullptr), sm_(nullptr), smgr_(nullptr), raft_instance_(nullptr) {}

    void reset() {
      raft_logger_.reset();
      sm_.reset();
      smgr_.reset();
      raft_instance_.reset();
    }

    // Server ID.
    int server_id_;

    // Server address.
    std::string addr_;

    // Server port.
    int port_;

    // Endpoint: `<addr>:<port>`.
    std::string endpoint_;

    // Logger.
    ptr<logger> raft_logger_;

    // State machine.
    ptr<state_machine> sm_;

    // State manager.
    ptr<state_mgr> smgr_;

    // Raft launcher.
    raft_launcher launcher_;

    // Raft server instance.
    ptr<raft_server> raft_instance_;
  } server_stuff;

}  // namespace app::consensus

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
    static std::shared_ptr<std::string> master;                                       // master machine's address
    static app::consensus::server_stuff stuff;                                        // consensus server NuRaft configurations
  };

}  // namespace app

namespace app::server {
  // Used to store info on each lock being held currently
  struct Lock {
    std::string path;                                     // Path to the file
    LockStatus status;                                    // Is it shared or exclusive?
    std::shared_ptr<std::map<std::string, bool>> owners;  // Who owns the lock
    std::string content;                                  // File content
  };

  // Used to store info on active sessions with clients
  struct Session {
    std::string client_id;                                                // Id of the client with whom this session exists
    chrono::system_clock::time_point start_time;                          // Start time of the local session
    chrono::milliseconds lease_length;                                    // Length of the session lease
    std::shared_ptr<msd::channel<int>> block_reply;                       // Channel used for blocking the reply to keep_alive rpcs
    std::shared_ptr<std::map<std::string, std::shared_ptr<Lock>>> locks;  // Locks acquired with the session
    bool terminated;                                                      // Indicator of if the session has been terminated manually
    // bool response_ready;                                                  // The condition that will be blocked on when waiting to send response
    // std::mutex m;                                                         // Used with the condition variable to allow for waiting
    // std::condition_variable cv;                                           // Used to notify if it is time to respond to a keep alive rpc
    // std::shared_ptr<msd::channel<int>> terminate_indicator;               // Used to tell the session monitor if the session has been terminated
  };

  // Information on the server
  struct info {
    static std::shared_ptr<std::map<std::string, std::shared_ptr<Lock>>> locks;        // Map of filepaths to lock structures
    static std::shared_ptr<std::map<std::string, std::shared_ptr<Session>>> sessions;  // Map of client ids to sessions
    // static std::shared_ptr<std::map<std::string, std::thread>> session_managers;       // Map of client ids to session manager threads
  };

}  // namespace app::server

namespace app::client {
  // Information about the client
  struct info {
    static std::string session_id;  // Will be equal to the client's ip:port string for simplicity
    // static std::thread session;                                       // The thread in which the session is managed
    static chrono::system_clock::time_point lease_start;              // The start time of the local lease
    static chrono::milliseconds lease_length;                         // Time remaining on the local lease
    static std::shared_ptr<std::map<std::string, LockStatus>> locks;  // LockStatus is declared in interface.proto so it can be sent in rpcs
    static bool jeopardy;                                             // Are we in jeopardy right now? (i.e. master could have died)
    static bool expired;                                              // Is the lease expired?
    static std::shared_ptr<Node> master;                              // Easy reference to the master's node
  };

}  // namespace app::client