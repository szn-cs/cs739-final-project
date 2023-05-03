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
    static std::shared_ptr<std::string> master;                                       // master machine's address

  };

  void initializeStaticInstance(std::shared_ptr<utility::parse::Config> config, std::vector<std::string> addressList);
  bool createNode(const std::string &file_name, bool is_dir, uint64_t *instance_number);

}  // namespace app

namespace app::server {
  // Used to store info on each lock being held currently
  struct Lock{
    std::string path;                     // Path to the file
    LockStatus status;                    // Is it shared or exclusive?
    std::map<std::string, bool> owners;   // Who owns the lock
    std::string content;                  // File content
  };

  // Used to store info on active sessions with clients
  struct Session{
    std::string client_id;                           // Id of the client with whom this session exists
    chrono::system_clock::time_point start_time;     // Start time of the local session
    chrono::milliseconds lease_length;               // Length of the session lease
    msd::channel<int> block_reply;                   // Channel used for blocking the reply to keep_alive rpcs
    std::map<std::string, Lock> locks;               // Locks acquired with the session
    bool terminated;                                 // Indicator of if the session has been terminated manually
    msd::channel<int> terminate_indicator;           // Used to tell the session monitor if the session has been terminated
  };

  // Information on the server
  struct info{
    static std::shared_ptr<std::map<std::string, std::shared_ptr<Lock>>> locks;       // Map of filepaths to lock structures
    static std::shared_ptr<std::map<std::string, std::shared_ptr<Session>>> sessions; // Map of client ids to sessions
  };
}

namespace app::client {
  // Information about the client
  struct info{
    static std::string session_id;                                   // Will be equal to the client's ip:port string for simplicity
    static std::thread session;                                      // The thread in which the session is managed
    static chrono::system_clock::time_point lease_start;             // The start time of the local lease
    static chrono::milliseconds lease_length;                        // Time remaining on the local lease
    static std::shared_ptr<std::map<std::string, LockStatus>> locks; // LockStatus is declared in interface.proto so it can be sent in rpcs
    static bool jeopardy;                                            // Are we in jeopardy right now? (i.e. master could have died)
    static bool expired;                                             // Is the lease expired?
    static std::shared_ptr<Node> master;                             // Easy reference to the master's node
  };

  /**
   * @brief Called to start a session with the master server
   * 
   * First, this method polls all of the servers in the cluster, sending init_session rpcs.
   * Only the master will reply to this with rpc::Status::Ok
   * 
   * 
   * 
   */
  grpc::Status start_session();
}
