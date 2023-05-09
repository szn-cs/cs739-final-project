#pragma once

#include "library.h"
#include "rpc.h"
#include "struct.h"
#include "utility.h"

namespace app {

  void initializeStaticInstance(std::shared_ptr<utility::parse::Config> config, std::vector<std::string> addressList);
  bool createNode(const std::string &file_name, bool is_dir, uint64_t *instance_number);
  void init_consensus();

}  // namespace app

namespace app::server {

  void init_server_info();
  grpc::Status create_session(std::string);
  void maintain_session(std::shared_ptr<Session>);
  void end_session(std::shared_ptr<Session>);
  int64_t attempt_extend_session(std::string);  // TODO: Make a version that takes a map of locks, for when in jeopardy
  grpc::Status open_lock(std::string, std::string);
  grpc::Status delete_lock(std::string, std::string);

}  // namespace app::server

namespace app::client {

  /**
   * @brief Called to start a session with the master server
   * 
   * First, this method polls all of the servers in the cluster, sending init_session rpcs.
   * Only the master will reply to this with rpc::Status::Ok
   */
  grpc::Status start_session();
  void maintain_session();
  bool open_lock(std::string);
  bool delete_lock(std::string);

}  // namespace app::client
